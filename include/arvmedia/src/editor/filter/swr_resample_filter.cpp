//
// Created by jerett on 2017/6/9.
//

#include "swr_resample_filter.h"
#include <memory>
#include "av_toolbox/swr_resampler.h"

namespace ins {

SwrResampleFilter::SwrResampleFilter(uint64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate)
    :resampler_(new SwrResampler(out_ch_layout, out_sample_fmt, out_sample_rate)){
}

SwrResampleFilter::~SwrResampleFilter() = default;

bool SwrResampleFilter::Init(MediaContext *ctx) {
  ctx->output_stream.codecpar->channel_layout = resampler_->out_ch_layout();
  ctx->output_stream.codecpar->format = resampler_->out_sample_fmt();
  ctx->output_stream.codecpar->sample_rate = resampler_->out_sample_rate();
  return next_filter_->Init(ctx);
}

bool SwrResampleFilter::Filter(MediaContext *ctx, const sp<AVFrame> &frame) {
  if (!resampler_->Init(frame)) return false;
  sp<AVFrame> out_frame;
  auto ret = resampler_->ResampleFrame(frame, out_frame);
  return ret == 0 && next_filter_->Filter(ctx, out_frame);
}

void SwrResampleFilter::Close(MediaContext *ctx) {
  resampler_.reset();
  return next_filter_->Close(ctx);
}


}
