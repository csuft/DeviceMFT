//
// Created by jerett on 2017/6/9.
//

#ifndef INSPLAYER_SWR_RESAMPLER_H
#define INSPLAYER_SWR_RESAMPLER_H

#include "sp.h"
extern "C" {
#include <libswresample/swresample.h>
}

struct AVFrame;
struct SwrContext;

namespace ins {

class SwrResampler {
public:
  SwrResampler(uint64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate) noexcept;

  bool Init(const sp<AVFrame> &src_frame);
  int ResampleFrame(const sp<AVFrame> &src_frame, sp<AVFrame> &out_frame);

  uint64_t out_ch_layout() const {
    return out_ch_layout_;
  }

  AVSampleFormat out_sample_fmt() const {
    return out_sample_fmt_;
  }

  int out_sample_rate() const {
    return out_sample_rate_;
  }

private:
  sp<SwrContext> swr_ctx_;
  uint64_t out_ch_layout_ = 0;
  AVSampleFormat out_sample_fmt_;
  int out_sample_rate_ = -1;
  sp<SwrContext> ctx_;

};

}


#endif //INSPLAYER_SWR_RESAMPLER_H
