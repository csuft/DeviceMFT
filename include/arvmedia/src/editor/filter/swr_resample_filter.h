//
// Created by jerett on 2017/6/9.
//

#ifndef INSPLAYER_SWR_RESAMPLE_FILTER_H
#define INSPLAYER_SWR_RESAMPLE_FILTER_H

#include "media_filter.h"
extern "C" {
#include <libavformat/avformat.h>
}

namespace ins {

class SwrResampler;

class SwrResampleFilter : public AudioFrameFilter<sp<AVFrame>> {
public:
  SwrResampleFilter(uint64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate);
  ~SwrResampleFilter();

  bool Init(MediaContext *ctx) override;
  bool Filter(MediaContext *ctx, const sp<AVFrame> &frame) override;
  void Close(MediaContext *ctx) override;

private:
  sp<SwrResampler> resampler_;
};

}


#endif //INSPLAYER_SWR_RESAMPLE_FILTER_H
