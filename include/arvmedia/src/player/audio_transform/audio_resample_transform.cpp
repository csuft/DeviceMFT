//
// Created by jerett on 2017/6/14.
//

#include "audio_resample_transform.h"
#include "av_toolbox/swr_resampler.h"

namespace ins {


AudioResampleTransform::AudioResampleTransform(uint64_t out_ch_layout,
                                               AVSampleFormat out_sample_fmt,
                                               int out_sample_rate)
    :resampler_(new SwrResampler(out_ch_layout, out_sample_fmt, out_sample_rate)){
}

AudioResampleTransform::~AudioResampleTransform() = default;

bool AudioResampleTransform::Init(AudioTransformContext *ctx) {
  ctx->out_codecpar->channel_layout = resampler_->out_ch_layout();
  ctx->out_codecpar->format = resampler_->out_sample_fmt();
  ctx->out_codecpar->sample_rate = resampler_->out_sample_rate();
  return !next_transform_ || next_transform_->Init(ctx);
}

bool AudioResampleTransform::Transform(sp<AVFrame> in, sp<AVFrame> &out) {
  if (!resampler_->Init(in)) return false;
  auto ret = resampler_->ResampleFrame(in, out);
  return ret == 0 && (!next_transform_ || next_transform_->Transform(out, out));
}

}