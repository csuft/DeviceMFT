#include "frame_util.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#if (__APPLE__)
#include <libavcodec/videotoolbox.h>
#endif
}
#include <llog/llog.h>

namespace ins {

sp<AVFrame> NewAVFrame() {
  auto frame = av_frame_alloc();
  CHECK(frame != nullptr) << "alloc frame nullptr";
  sp<AVFrame> sp_frame(frame, [](AVFrame *f) {
    av_frame_free(&f);
  });
  return sp_frame;
}

sp<AVFrame> NewAVFrame(int width, int height, AVPixelFormat fmt, int align) {
  auto frame = av_frame_alloc();
  frame->width = width;
  frame->height = height;
  frame->format = fmt;
  av_frame_get_buffer(frame, align);
  CHECK(frame != nullptr) << "alloc frame nullptr";
  sp<AVFrame> sp_frame(frame, [](AVFrame *f) {
    av_frame_free(&f);
  });
  return sp_frame;
}

sp<AVFrame> CloneAVFrame(const sp<AVFrame> &src) {
  auto dst_frame = av_frame_clone(src.get());
  CHECK(dst_frame != nullptr);
  sp<AVFrame> sp_frame(dst_frame, [](AVFrame *frame) {
    av_frame_free(&frame);
  });
  return sp_frame;
}

sp<AVFrame> NewCUDAFrame(AVBufferRef *hw_frames_ctx) {
  auto cuda_frame = NewAVFrame();
  av_hwframe_get_buffer(hw_frames_ctx, cuda_frame.get(), 0);
  return cuda_frame;
}


#if (__APPLE__)
sp<AVFrame> CopyCVPixelBufferToFrame(CVPixelBufferRef pixbuf) {
  sp<AVFrame> out_frame = NewAVFrame();
  OSType pixel_format = CVPixelBufferGetPixelFormatType(pixbuf);
  CVReturn err;
  uint8_t *data[4] = { 0 };
  int linesize[4] = { 0 };

  switch (pixel_format) {
    case kCVPixelFormatType_420YpCbCr8Planar: out_frame->format = AV_PIX_FMT_YUV420P; break;
    case kCVPixelFormatType_422YpCbCr8:       out_frame->format = AV_PIX_FMT_UYVY422; break;
    case kCVPixelFormatType_32BGRA:           out_frame->format = AV_PIX_FMT_BGRA; break;
#ifdef kCFCoreFoundationVersionNumber10_7
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: out_frame->format = AV_PIX_FMT_NV12; break;
#endif
    default:
      LOG(ERROR) << "Unsupported pixel format:" << pixel_format;
      return nullptr;
  }

  out_frame->width = static_cast<int>(CVPixelBufferGetWidth(pixbuf));
  out_frame->height = static_cast<int>(CVPixelBufferGetHeight(pixbuf));
  auto ret = av_frame_get_buffer(out_frame.get(), 32);
  if (ret < 0)
    return nullptr;

  err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
  if (err != kCVReturnSuccess) {
    LOG(ERROR) << "Error locking the pixel buffer.";
    return nullptr;
  }

  if (CVPixelBufferIsPlanar(pixbuf)) {
    auto planes = CVPixelBufferGetPlaneCount(pixbuf);
    for (size_t i = 0; i < planes; i++) {
      data[i] = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
      linesize[i] = static_cast<int>(CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i));
    }
  } else {
    data[0] = (uint8_t *)CVPixelBufferGetBaseAddress(pixbuf);
    linesize[0] = static_cast<int>(CVPixelBufferGetBytesPerRow(pixbuf));
  }

  av_image_copy(out_frame->data, out_frame->linesize,
    (const uint8_t **)data, linesize, (AVPixelFormat)out_frame->format,
                out_frame->width, out_frame->height);

  CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
  if (ret < 0)
    return nullptr;
  return out_frame;
}

static void videotoolbox_buffer_release(void *opaque, uint8_t *data) {
  CVPixelBufferRef cv_buffer = reinterpret_cast<CVPixelBufferRef>(data);
  CVPixelBufferRelease(cv_buffer);
  //  LOG(VERBOSE) << "fuck ......................!!!! release .";
}

sp<AVFrame> RefCVPixelBufferToFrame(CVPixelBufferRef pixel) {
  sp<AVFrame> out_frame = NewAVFrame();
  out_frame->format = AV_PIX_FMT_VIDEOTOOLBOX;
  out_frame->width = static_cast<int>(CVPixelBufferGetWidth(pixel));
  out_frame->height = static_cast<int>(CVPixelBufferGetHeight(pixel));
  out_frame->buf[0] = av_buffer_create((uint8_t*)pixel,
                                       sizeof(pixel),
                                       videotoolbox_buffer_release,
                                       nullptr,
                                       AV_BUFFER_FLAG_READONLY);
  if (!out_frame->buf[0]) {
    return nullptr;
  }
  out_frame->data[3] = (uint8_t*)pixel;
  return out_frame;
}
#endif

sp<AVFrame> NewAudioFrame(uint64_t layout, int sample_rate, int nb_samples, AVSampleFormat sample_format) {
  auto frame = av_frame_alloc();
  frame->channel_layout = layout;
  frame->sample_rate = sample_rate;
  frame->format = sample_format;
  frame->nb_samples = nb_samples;

  av_frame_get_buffer(frame, 1);
  sp<AVFrame> sp_audio_frame(frame, [](AVFrame *f) {
    av_frame_free(&f);
  });
  return sp_audio_frame;
}

}
