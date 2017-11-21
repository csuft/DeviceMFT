//
// Created by jerett on 2017/6/9.
//

#include "swr_resampler.h"
#include "ffmpeg_util.h"
#include "llog/llog.h"

namespace ins {

SwrResampler::SwrResampler(uint64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate) noexcept
    :out_ch_layout_(out_ch_layout), out_sample_fmt_(out_sample_fmt), out_sample_rate_(out_sample_rate) {
}

bool SwrResampler::Init(const sp<AVFrame> &src_frame) {
  if (ctx_ == nullptr) {
    auto in_ch_layout = src_frame->channel_layout;
    auto in_fmt = static_cast<AVSampleFormat>(src_frame->format);
    auto in_sample_rate = src_frame->sample_rate;
    ctx_ = NewSwrContext(out_ch_layout_, out_sample_fmt_, out_sample_rate_,
                         in_ch_layout, in_fmt, in_sample_rate,
                         0, nullptr);
    auto ret = swr_init(ctx_.get());
    return ret == 0;
  } else {
    return true;
  }
}

int SwrResampler::ResampleFrame(const sp<AVFrame> &src_frame, sp<AVFrame> &out_frame) {
  auto sp_resample_frame = NewAudioFrame(out_ch_layout_, out_sample_rate_, src_frame->nb_samples, out_sample_fmt_);
  auto ret = swr_convert_frame(ctx_.get(), sp_resample_frame.get(), src_frame.get());
//  CHECK(ret == 0) << FFmpegErrorToString(ret);
  av_frame_copy_props(sp_resample_frame.get(), src_frame.get());
  out_frame = sp_resample_frame;
  return ret;
}


}
