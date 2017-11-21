#pragma once
#include "platform.h"
extern "C" {
#include <libavformat/avformat.h>
#if ON_APPLE
#include <libavcodec/videotoolbox.h>
#endif
}
#include "sp.h"

struct AVFrame;

namespace ins {

sp<AVFrame> NewAVFrame();
sp<AVFrame> NewAVFrame(int width, int height, AVPixelFormat fmt, int align = 32);
sp<AVFrame> CloneAVFrame(const sp<AVFrame> &src);
sp<AVFrame> NewCUDAFrame(AVBufferRef *hw_frames_ctx);

#if ON_APPLE
/// copy CVPixelBuffer to corresponding frame,  will use memcpy
/// @param apple pixel 
/// @return frame
sp<AVFrame> CopyCVPixelBufferToFrame(CVPixelBufferRef pixel);

/// ref CVPixelBuffer to corresponding frame,  don't use memcpy
/// @param pixel pixel buffer
/// @return frame
sp<AVFrame> RefCVPixelBufferToFrame(CVPixelBufferRef pixel);
#endif

sp<AVFrame> NewAudioFrame(uint64_t layout, int sample_rate, int nb_samples, AVSampleFormat sample_format);
}
