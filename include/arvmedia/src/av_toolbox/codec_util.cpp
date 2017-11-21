#include "codec_util.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#if ON_APPLE
#include <libavcodec/videotoolbox.h>
#endif
}
#include <llog/llog.h>

namespace ins {

extern std::string FFmpegErrorToString(int code);
#if ON_APPLE
AVPixelFormat UseVideoToolboxPixelFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts);
#endif // __APPLE__

#if ON_WIN64
AVPixelFormat CuvidUseCudaPixelFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts);
AVPixelFormat CuvidUseNV12PixelFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts);
#endif

sp<AVCodecParameters> NewAVCodecParameters() {
  auto par = avcodec_parameters_alloc();
  CHECK(par != nullptr);
  sp<AVCodecParameters> sp_cp(par, [](AVCodecParameters *p) {
                            avcodec_parameters_free(&p);
                          });
  return sp_cp;
}

sp<AVCodecContext> NewAVCodecContext(AVCodec *codec) {
  CHECK(codec != nullptr);
  auto ctx = avcodec_alloc_context3(codec);
  CHECK(ctx != nullptr) << "alloc context3 retrun nullptr";
  sp<AVCodecContext> sp_ctx(ctx, [](AVCodecContext *c) {
                          avcodec_close(c);
                          avcodec_free_context(&c);
                        });
  return sp_ctx;
}

sp<AVCodecContext> NewAVDecodeContextFromID(AVCodecID decoder_id) {
  auto codec = avcodec_find_decoder(decoder_id);
  if (codec == nullptr) {
    LOG(WARNING) << "unable to find decoder ID:" << decoder_id;
    return nullptr;
  }
  auto ctx = avcodec_alloc_context3(codec);
  sp<AVCodecContext> sp_ctx(ctx, [](AVCodecContext *c) {
                          if (c->pix_fmt == AV_PIX_FMT_CUDA && c->opaque) {
                            auto hw_frame_ctx = static_cast<AVBufferRef*>(c->opaque);
                            av_buffer_unref(&hw_frame_ctx);
                            LOG(VERBOSE) << "unref cuvid decode opaque.";
                          }
                          avcodec_free_context(&c);
                        });
  return sp_ctx;
}


sp<AVCodecContext> NewAVDecodeContextFromName(const std::string &name) {
  auto codec = avcodec_find_decoder_by_name(name.c_str());
  if (codec == nullptr) {
    LOG(WARNING) << "unable to find decoder:" << name;
    return nullptr;
  }
  auto ctx = avcodec_alloc_context3(codec);
  sp<AVCodecContext> sp_ctx(ctx, [](AVCodecContext *c) {
                          if (c->pix_fmt == AV_PIX_FMT_CUDA && c->opaque) {
                            auto hw_frame_ctx = static_cast<AVBufferRef*>(c->opaque);
                            av_buffer_unref(&hw_frame_ctx);
                            LOG(VERBOSE) << "unref cuvid decode opaque.";
                          }
                          avcodec_free_context(&c);
                        });
  return sp_ctx;
}

sp<AVCodecContext> NewAVHwaccelDecodeContextFromID(AVCodecID id, bool copy_hwaccel) {
  //atempt use videotoolbox, this must be after avcodec_parameters_to_context method.
#if (__APPLE__)
  if (id == AV_CODEC_ID_H264) {
    auto ctx = NewAVDecodeContextFromID(AV_CODEC_ID_H264);
    if (ctx) {
      ctx->pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
      ctx->get_format = UseVideoToolboxPixelFormat;
    }
    return ctx;
  }
#elif (_WIN32)
  if (id == AV_CODEC_ID_H264) {
    auto ctx = NewAVDecodeContextFromName("h264_cuvid");
    if (ctx) {
      if (!copy_hwaccel) {
        ctx->pix_fmt = AV_PIX_FMT_CUDA;
        ctx->sw_pix_fmt = AV_PIX_FMT_NV12;
        ctx->get_format = CuvidUseCudaPixelFormat;
      } else {
        ctx->pix_fmt = AV_PIX_FMT_NV12;
        ctx->get_format = CuvidUseNV12PixelFormat;
      }
    }
    return ctx;
  }
#endif
  return nullptr;
}

sp<AVCodecContext> NewAVEncodeContextFromID(AVCodecID id) {
  auto codec = avcodec_find_encoder(id);
  if (codec == nullptr) {
    LOG(WARNING) << "unable to find encoder, ID:" << id;
    return nullptr;
  }
  AVCodecContext *ctx = avcodec_alloc_context3(codec);
  sp<AVCodecContext> sp_ctx(ctx, [](AVCodecContext *c) {
    avcodec_free_context(&c);
  });
  return sp_ctx;
}

sp<AVCodecContext> NewAVHwaccelEncodeContextFromID(AVCodecID id) {
#if _WIN32
  if (id == AV_CODEC_ID_H264) return NewAVEncodeContextFromName("h264_nvenc");
  if (id == AV_CODEC_ID_H265) return NewAVEncodeContextFromName("hevc_nvenc");
#elif __APPLE__
  if (id == AV_CODEC_ID_H264) return NewAVEncodeContextFromName("h264_videotoolbox");
#endif // _WIN32
  return nullptr;
}

sp<AVCodecContext> NewAVEncodeContextFromName(const std::string &name) {
  auto codec = avcodec_find_encoder_by_name(name.c_str());
  if (codec == nullptr) {
    LOG(WARNING) << "unable to find encoder name:" << name;
    return nullptr;
  }
  AVCodecContext *ctx = avcodec_alloc_context3(codec);
  sp<AVCodecContext> sp_ctx(ctx, [](AVCodecContext *c) {
    avcodec_close(c);
    avcodec_free_context(&c);
  });
  return sp_ctx;
}

}

namespace ins {
#if ON_APPLE
AVPixelFormat UseVideoToolboxPixelFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts) {
  auto ret = av_videotoolbox_default_init(s);
  if (ret < 0) {
    LOG(ERROR) << "videotoolbox init err:" << ins::FFmpegErrorToString(ret);
    LOG(VERBOSE) << "use h264 decode.";
    return AV_PIX_FMT_YUV420P;
  } else {
    LOG(VERBOSE) << "use videotoolbox decode.";
    return AV_PIX_FMT_VIDEOTOOLBOX;
  }
}
#endif // __APPLE__

#if ON_WIN64
AVPixelFormat CuvidUseCudaPixelFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts) {
  if (!s->opaque) {
    AVBufferRef *device_buffer = nullptr;
    //TODO:test when create device failed.
    auto ret = av_hwdevice_ctx_create(&device_buffer, AV_HWDEVICE_TYPE_CUDA, "cuvid", nullptr, 0);
    if (ret != 0) {
      LOG(ERROR) << "av_hwdevice_ctx_create failed:" << ins::FFmpegErrorToString(ret);
      return static_cast<AVPixelFormat>(-1);
    }
    auto hwframe_buffer = av_hwframe_ctx_alloc(device_buffer);
    CHECK(hwframe_buffer != nullptr);
    av_buffer_unref(&device_buffer);

    auto hwframe_ctx = reinterpret_cast<AVHWFramesContext*>(hwframe_buffer->data);
    hwframe_ctx->format = AV_PIX_FMT_CUDA;
    hwframe_ctx->sw_format = AV_PIX_FMT_NV12;
    s->hw_frames_ctx = av_buffer_ref(hwframe_buffer);

    s->opaque = hwframe_buffer;
    LOG(INFO) << "init cuda pixel format....";
  }
  return AV_PIX_FMT_CUDA;
}

AVPixelFormat CuvidUseNV12PixelFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts) {
  return AV_PIX_FMT_NV12;
}
#endif
}
