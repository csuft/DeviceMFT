#include "decode_filter.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}
#include "llog/llog.h"
#include "av_toolbox/ffmpeg_util.h"
#include "editor/editor_error.h"

namespace ins {

bool DecodeFilter::InitHardwareDecoder(MediaContext *ctx) {
  auto stream = ctx->input_stream.stream;
  CHECK(stream != nullptr);
  time_base_ = stream->time_base;
  enable_hwaccel_ = true;
  dec_ctx_ = NewAVHwaccelDecodeContextFromID(stream->codecpar->codec_id, copy_hwaccel_);
  if (!dec_ctx_) {
    LOG(VERBOSE) << "find hwaccel ctx failed.";
    return false;
  }
  LOG(VERBOSE) << "to use hwaccel decoder:" << dec_ctx_->codec->name;
  //cpy fmt and reset this is for videotoolbox decode.
  auto fmt_cpy = dec_ctx_->pix_fmt;
  avcodec_parameters_to_context(dec_ctx_.get(), stream->codecpar);
  dec_ctx_->pix_fmt = fmt_cpy;
  dec_ctx_->refcounted_frames = 1;
  decoder_.reset(new Decoder(dec_ctx_));
  auto ret = decoder_->Open();
  if (ret == 0 && !copy_hwaccel_ && dec_ctx_->hw_frames_ctx) {
    LOG(VERBOSE) << "set decode hw_frame_ctx.";
    //ctx->input_stream.hw_frame_ctx = dec_ctx_->hw_frames_ctx;
    ctx->input_stream.hw_frame_ctx = av_buffer_ref(dec_ctx_->hw_frames_ctx);
  }
  LOG_IF(WARNING, ret != 0) << "open hwaccel decoder failed.";
  return ret == 0;
}

bool DecodeFilter::InitSoftwareDecoder(MediaContext *ctx) {
  auto stream = ctx->input_stream.stream;
  CHECK(stream != nullptr);
  time_base_ = stream->time_base;
  enable_hwaccel_ = false;
  dec_ctx_ = NewAVDecodeContextFromID(stream->codecpar->codec_id);
  LOG(VERBOSE) << "change to decoder:" << dec_ctx_->codec->name;
  dec_ctx_->refcounted_frames = 1;
  if (threads_ >= 0) dec_ctx_->thread_count = threads_;
  CHECK(avcodec_parameters_to_context(dec_ctx_.get(), stream->codecpar) == 0);
  decoder_.reset(new Decoder(dec_ctx_));
  auto ret = decoder_->Open();
  LOG_IF(INFO, ret != 0) << "open soft decoder failed.";
  return ret == 0;
}

bool DecodeFilter::Test(const sp<ARVPacket> &test_pkt, sp<AVFrame> &out_test_frame) {
  if (test_pkt == nullptr) return true;
  if (decoder_->SendPacket(test_pkt) != 0) {
    LOG(ERROR) << "test decoder:" << dec_ctx_->codec->name << " failed.";
    return false;
  }
  decoder_->SendEOF();
  auto ret = decoder_->ReceiveFrame(out_test_frame);
  LOG_IF(WARNING, ret != 0) << "test decoder:" << dec_ctx_->codec->name << " failed.";
  return ret == 0;
}

bool DecodeFilter::Init(MediaContext *ctx) {
  CHECK(next_filter_ != nullptr);
  //first atemp use hardware decoder, switch to CPU decode otherwise
  sp<AVFrame> out_test_frame;
  if (enable_hwaccel_) {
    if (!InitHardwareDecoder(ctx)) return false;
  } else {
    if (!InitSoftwareDecoder(ctx)) return false;
  }
  CHECK(avcodec_parameters_from_context(ctx->output_stream.codecpar.get(), dec_ctx_.get()) >= 0);
  auto ret = next_filter_->Init(ctx);
  return ret;
}

bool DecodeFilter::Filter(MediaContext *ctx, const sp<ARVPacket> &pkt) {
  auto ret = decoder_->SendPacket(pkt);
  if (ret < 0) {
    return false;
  }
  sp<AVFrame> frame;
  ret = decoder_->ReceiveFrame(frame);
  if (ret == 0) {
    //LOG(INFO) << "decode frame size:" << picture->width << " " << picture->height
    //  << " time:" << TimestampToMs(picture->pts, time_base_)
    //  << " pix_fmt:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(picture->format))->name;
    return next_filter_->Filter(ctx, frame);
  }
  else if (ret == AVERROR(EAGAIN)) {
    return true;
  }
  else {
    ctx->error_code = kDecodeError;
    return false;
  }
}

void DecodeFilter::Close(MediaContext *ctx) {
  if (ctx->input_stream.hw_frame_ctx) {
    av_buffer_unref(&ctx->input_stream.hw_frame_ctx);
  }
  LOG(VERBOSE) << "DecodeFilter close";
  next_filter_->Close(ctx);
}

void DecodeFilter::Notify(MediaContext *ctx, const MediaNotify &notify) {
  if (notify.type == kNotifyEOF) {
    LOG(VERBOSE) << "DecodeFilter flush buffer";
    decoder_->SendEOF();
    sp<AVFrame> picture;
    while (decoder_->ReceiveFrame(picture) != AVERROR_EOF) {
      //LOG(VERBOSE) << "flush frame size:" << picture->width << " " << picture->height
      //  << " time:" << TimestampToMs(picture->pts, time_base_);
      if (!next_filter_->Filter(ctx, picture)) break;
    }
  }
  return next_filter_->Notify(ctx, notify);
}

}
