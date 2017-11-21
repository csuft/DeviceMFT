#pragma once

#include "media_filter.h"
#include <llog/llog.h>

namespace ins {

template <typename DataType>
class StatisticFilter : public MediaFilter<DataType, DataType> {
public:
  bool Init(MediaContext *ctx) override;
  bool Filter(MediaContext *ctx, const DataType &data) override;
  void Close(MediaContext *ctx) override;
  void Notify(MediaContext *ctx, const MediaNotify &notify) override;

private:
  int log_counter_ = 0;
  double fps_ = 0;
  Timer fps_timer_;
};

template <typename DataType>
bool StatisticFilter<DataType>::Init(MediaContext *ctx) {
  fps_timer_.Reset();
  return this->next_filter_->Init(ctx);
}

template <typename DataType>
bool StatisticFilter<DataType>::Filter(MediaContext *ctx, const DataType &data) {
  {
    //fps statistic
    ++log_counter_;
    auto pass_ms = fps_timer_.Pass();
    if (pass_ms >= 1000) {
      fps_ = static_cast<double>(log_counter_) * 1000 / pass_ms;
      fps_timer_.Reset();
      log_counter_ = 0;
      LOG(VERBOSE) << "src index " << ctx->input_stream.index << ", fps:" << fps_;
    }
  }
  return this->next_filter_->Filter(ctx, data);
}

template <typename DataType>
void StatisticFilter<DataType>::Close(MediaContext *ctx) {
  return this->next_filter_->Close(ctx);
}

template <typename DataType>
void StatisticFilter<DataType>::Notify(MediaContext *ctx, const MediaNotify &notify) {
  if (notify.type == kNotifyEOF) {
    LOG(VERBOSE) << "src index " << ctx->input_stream.index << " notify eof.";
  } else if (notify.type == kNotifyFirstVideoPktDts) {
    auto dts = any_cast<int64_t>(notify.user_data);
    LOG(VERBOSE) << "src index " << ctx->input_stream.index
      << ",notify first video pkt dts in ms:" << TimestampToMs(dts, ctx->input_stream.stream->time_base);
  } else if (notify.type == kNotifyFirstAudioPktDts) {
    auto dts = any_cast<int64_t>(notify.user_data);
    LOG(VERBOSE) << "src index " << ctx->input_stream.index
      << ",notify first audio pkt dts in ms:" << TimestampToMs(dts, ctx->input_stream.stream->time_base);
  }
  return this->next_filter_->Notify(ctx, notify);
}

}

