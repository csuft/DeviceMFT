//
// Created by jerett on 16/8/11.
//

#include "encode_filter.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#if (__APPLE__)
#include <libavcodec/videotoolbox.h>
#endif
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
};

#include "av_toolbox/any.h"
#include "av_toolbox/ffmpeg_util.h"
#include "editor/editor_error.h"

namespace ins {

bool EncodeFilter::InitHwaccelEncoder(MediaContext *ctx) {
  enable_hwaccel_ = true;
  enc_ctx_ = NewAVHwaccelEncodeContextFromID(encodec_id_);
  if (!enc_ctx_) {
    LOG(VERBOSE) << "find hwaccel encode ctx failed.";
    return false;
  }
  LOG(VERBOSE) << "to use hwaccel encoder:" << enc_ctx_->codec->name;
  if (!ConfigureEncContext(ctx)) {
    LOG(VERBOSE) << "config hwaccel encoder failed.";
    return false;
  }
  return true;
}

bool EncodeFilter::InitSoftwareEncoder(MediaContext *ctx) {
  enable_hwaccel_ = false;
  enc_ctx_ = NewAVEncodeContextFromID(encodec_id_);
  if (!enc_ctx_) {
    LOG(VERBOSE) << "find soft encode ctx failed.";
    return false;
  }
  LOG(VERBOSE) << "switch to use encoder:" << enc_ctx_->codec->name;
  if (!ConfigureEncContext(ctx)) {
    LOG(VERBOSE) << "config soft encoder failed.";
    return false;
  }
  return true;
}

bool EncodeFilter::ConfigureEncContext(MediaContext *ctx) {
  if (enc_ctx_->codec_type == AVMEDIA_TYPE_VIDEO) {
    //common param setting
    {
      stream_index_ = ctx->input_stream.stream->index;
      enc_ctx_->width = ctx->output_stream.codecpar->width;
      enc_ctx_->height = ctx->output_stream.codecpar->height;
      enc_ctx_->pix_fmt = static_cast<AVPixelFormat>(ctx->output_stream.codecpar->format);
      enc_ctx_->time_base = ctx->input_stream.stream->time_base;
      enc_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
      if (bitrate_ != -1) enc_ctx_->bit_rate = bitrate_;
      if (bframes_ != -1) enc_ctx_->max_b_frames = bframes_;
      if (threads_ != -1) enc_ctx_->thread_count = threads_;
      if (gop_size_ != -1) enc_ctx_->gop_size = gop_size_;
    }

    //encoder type check
    if (std::strcmp(enc_ctx_->codec->name, "libx264") == 0) encoder_type_ = kEncoderX264;
    if (std::strcmp(enc_ctx_->codec->name, "libx265") == 0) encoder_type_ = kEncoderX265;
    if (std::strcmp(enc_ctx_->codec->name, "h264_videotoolbox") == 0) encoder_type_ = kEncoderVideoToolbox;
    if (std::strcmp(enc_ctx_->codec->name, "h264_nvenc") == 0) encoder_type_ = kEncoderH264NVENC;
    if (std::strcmp(enc_ctx_->codec->name, "hevc_nvenc") == 0) encoder_type_ = kEncoderH265NVENC;

    auto ffmpeg_videotoolbox_bug = (encoder_type_ == kEncoderVideoToolbox &&
                                    ctx->output_stream.codecpar->format == AV_PIX_FMT_VIDEOTOOLBOX);
    switch (encoder_type_) {
      case kEncoderX264:
      {
        if (!preset_.empty()) av_opt_set(enc_ctx_->priv_data, "preset", preset_.c_str(), 0);
        if (!tune_.empty()) av_opt_set(enc_ctx_->priv_data, "tune", tune_.c_str(), 0);
        if (bitrate_ != -1) av_opt_set(enc_ctx_->priv_data, "x264-params", "force-cfr=1", 0);
        //handle use videotoolbox decode and use libx264 encode
        if (ctx->output_stream.codecpar->format == AV_PIX_FMT_VIDEOTOOLBOX ||
            ctx->output_stream.codecpar->format == AV_PIX_FMT_CUDA) {
          enc_ctx_->pix_fmt = AV_PIX_FMT_NV12;
        }
        auto frame_rate = ctx->input_stream.stream->avg_frame_rate;
        enc_ctx_->time_base = { frame_rate.den, frame_rate.num };
        break;
      }

      case kEncoderX265:
      {
        if (!preset_.empty()) av_opt_set(enc_ctx_->priv_data, "preset", preset_.c_str(), 0);
        if (!tune_.empty()) av_opt_set(enc_ctx_->priv_data, "tune", tune_.c_str(), 0);
        if (bitrate_ != -1) av_opt_set(enc_ctx_->priv_data, "x265-params", "force-cfr=1", 0);
        //TODO:handle use videotoolbox decode and use libx265 encode
        if (ctx->output_stream.codecpar->format == AV_PIX_FMT_VIDEOTOOLBOX ||
            ctx->output_stream.codecpar->format == AV_PIX_FMT_CUDA) {
          //enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
          LOG(ERROR) << "libx265 don't support cuda frame, pleaase scale first";
          return false;
        }
        auto frame_rate = ctx->input_stream.stream->avg_frame_rate;
        enc_ctx_->time_base = { frame_rate.den, frame_rate.num };
        break;
      }

      case kEncoderVideoToolbox:
      {
        av_opt_set_int(enc_ctx_->priv_data, "realtime", videotoolbox_realtime_, 0);
        break;
      }

      case kEncoderH264NVENC:
      case kEncoderH265NVENC:
      {
        auto frame_rate = ctx->input_stream.stream->avg_frame_rate;
        enc_ctx_->time_base = { frame_rate.den, frame_rate.num };
        if (ctx->output_stream.codecpar->format == AV_PIX_FMT_CUDA) {
          LOG(INFO) << "config nvidia enc hwacceltrue ctx";
          AVBufferRef *device_ref = nullptr;
          if (ctx->input_stream.hw_frame_ctx == nullptr) {
            auto ret = av_hwdevice_ctx_create(&device_ref, AV_HWDEVICE_TYPE_CUDA, "cuvid", nullptr, 0);
            if (ret != 0) {
              return false;
            }
          } else {
            device_ref = reinterpret_cast<AVHWFramesContext*>(ctx->input_stream.hw_frame_ctx->data)->device_ref;
          }

          CHECK(device_ref != nullptr);
          auto hwframe_ctx_ref = av_hwframe_ctx_alloc(device_ref);
          CHECK(hwframe_ctx_ref != nullptr);
          auto hwframe_ctx = reinterpret_cast<AVHWFramesContext*>(hwframe_ctx_ref->data);
          hwframe_ctx->format = AV_PIX_FMT_CUDA;
          hwframe_ctx->sw_format = AV_PIX_FMT_NV12;
          hwframe_ctx->width = enc_ctx_->width;
          hwframe_ctx->height = enc_ctx_->height;
          auto ret = av_hwframe_ctx_init(hwframe_ctx_ref);
          if (ret != 0) {
            LOG(ERROR) << "av_hwframe_ctx_init ret:" << ret << ", err:" << FFmpegErrorToString(ret);
            return false;
          }
          enc_ctx_->hw_frames_ctx = hwframe_ctx_ref;
          enc_ctx_->pix_fmt = AV_PIX_FMT_CUDA;
          enc_ctx_->sw_pix_fmt = AV_PIX_FMT_NV12;

          //update ctx hw_frames_ctx
          ctx->output_stream.hw_frame_ctx = hwframe_ctx_ref;
        }
        break;
      }
    }

    if (ffmpeg_videotoolbox_bug) {
      //绕过ffmpeg的生成extradata的bug
      enc_ctx_->pix_fmt = AV_PIX_FMT_NV12;
    }

    encoder_.reset(new Encoder(enc_ctx_));
    if (encoder_->Open() != 0) {
      return false;
    }
    if (ffmpeg_videotoolbox_bug) {
      enc_ctx_->pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
    }
    return true;
  } else if (enc_ctx_->codec_type == AVMEDIA_TYPE_AUDIO) {
    avcodec_parameters_to_context(enc_ctx_.get(), ctx->output_stream.codecpar.get());
    encoder_.reset(new Encoder(enc_ctx_));
    LOG(INFO) << "to use audio encoder.";
    if (encoder_->Open() != 0) {
      return false;
    }
    return true;
  }
  return true;
}

bool EncodeFilter::Test(MediaContext *ctx, const sp<AVFrame> &test_frame, sp<ARVPacket> &out_test_pkt) {
  if (!test_frame) return true;
  auto frame = PreprocessInputFrame(test_frame);
  auto ret = encoder_->SendFrame(frame);
  if (ret != 0) return false;
  encoder_->SendEOF();
  auto got_pkt = encoder_->ReceivePacket(out_test_pkt);
  if (got_pkt == 0) {
    //reopen encoder.
    LOG(VERBOSE) << "encoder test passed, need reopen encoder.";
    Reset(ctx);
    return true;
  } else {
    return false;
  }
}

bool EncodeFilter::Reset(MediaContext *ctx) {
  TimedBlock(obj, "Encoder Reset time");
  encoder_.reset();
  if (enable_hwaccel_) {
    enc_ctx_ = NewAVHwaccelEncodeContextFromID(encodec_id_);
  } else {
    enc_ctx_ = NewAVEncodeContextFromID(encodec_id_);
  }
  if (!ConfigureEncContext(ctx)) {
    LOG(ERROR) << "reopen encoder: configure ctx error";
    return false;
  }
  return true;
}

bool EncodeFilter::Init(MediaContext *ctx) {
  if (enable_hwaccel_) {
    if (!InitHwaccelEncoder(ctx)) return false;
  } else {
    if (!InitSoftwareEncoder(ctx)) return false;
  }
  CHECK(avcodec_parameters_from_context(ctx->output_stream.codecpar.get(), enc_ctx_.get()) == 0);
  return next_filter_->Init(ctx);
}

sp<AVFrame> EncodeFilter::PreprocessInputFrame(const sp<AVFrame> &in_frame) const {
  auto out_frame = in_frame;
  if (encoder_type_ == kEncoderX264 && in_frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
#if ON_APPLE
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)in_frame->data[3];
    out_frame = CopyCVPixelBufferToFrame(pixbuf);
    av_frame_copy_props(out_frame.get(), in_frame.get());
#else
    LOG(ERROR) << "how strange here.";
    exit(EXIT_FAILURE);
#endif
  }

  if (encoder_type_ == kEncoderH264NVENC &&
      in_frame->format == AV_PIX_FMT_CUDA &&
      in_frame->hw_frames_ctx != enc_ctx_->hw_frames_ctx) {
    //LOG(VERBOSE) << "clone cuda frame";
    //clone cuda frame
    auto cuda_frame = NewCUDAFrame(in_frame->hw_frames_ctx);
    av_frame_copy_props(cuda_frame.get(), in_frame.get());
    av_hwframe_transfer_data(cuda_frame.get(), in_frame.get(), 0);
    out_frame = cuda_frame;
  }

  if (encoder_type_ == kEncoderX264 && in_frame->format == AV_PIX_FMT_CUDA) {
    //auto nv12_frame = NewAVFrame(in_frame->width, in_frame->height, AV_PIX_FMT_NV12);
    auto nv12_frame = NewAVFrame();
    CHECK(av_hwframe_transfer_data(nv12_frame.get(), in_frame.get(), 0) == 0);
    av_frame_copy_props(nv12_frame.get(), in_frame.get());
    out_frame = nv12_frame;
  }

  return out_frame;
}

bool EncodeFilter::Filter(MediaContext *ctx, const sp<AVFrame> &in_frame) {
  auto frame = PreprocessInputFrame(in_frame);
  if (encoder_->SendFrame(frame) != 0) return false;
  sp<ARVPacket> pkt;
  auto ret = encoder_->ReceivePacket(pkt);
  if (ret == 0) {
    //LOG(VERBOSE) << "encode pkt pts:" << pkt->pts;
    pkt->stream_index = stream_index_;
    return next_filter_->Filter(ctx, pkt);
  } else if (ret == AVERROR(EAGAIN)) {
    return true;
  } else {
    ctx->error_code = kEncodeError;
    return false;
  }
}

void EncodeFilter::Close(MediaContext *ctx) {
  LOG(VERBOSE) << "EncodeFilter close";
  next_filter_->Close(ctx);
}

void EncodeFilter::Notify(MediaContext *ctx, const MediaNotify &notify) {
  if (notify.type == kNotifyEOF) {
    LOG(VERBOSE) << "EncodeFilter flush buffer";
    encoder_->SendEOF();
    sp<ARVPacket> pkt;
    while (encoder_->ReceivePacket(pkt) == 0) {
      pkt->stream_index = stream_index_;
      if (!next_filter_->Filter(ctx, pkt)) break;
    }
  }
  return next_filter_->Notify(ctx, notify);
}

}
