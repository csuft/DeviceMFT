//
// Created by jerett on 2017/6/14.
//

#pragma once

#include "audio_transform.h"
extern "C" {
#include <libavformat/avformat.h>
}

namespace ins {

class SwrResampler;
class AudioResampleTransform :public AudioTransform {
public:
  AudioResampleTransform(uint64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate);
  ~AudioResampleTransform();

  bool Init(AudioTransformContext *ctx) override;
  bool Transform(sp<AVFrame> in, sp<AVFrame> &out) override;

private:
  sp<SwrResampler> resampler_;
};

}

