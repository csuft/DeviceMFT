// Implementation of the circular buffer filter

// Copyright (c) 2017 insta360, jertt
#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <llog/llog.h>
#include <av_toolbox/dynamic_circular_buffer.h>
#include "media_filter.h"

namespace ins {

template <typename DataType>
class CircularBufferFilter : public MediaFilter<DataType, DataType> {
public:
  CircularBufferFilter(int capacity) : circular_buffer_(capacity) {
    ;
  }
  ~CircularBufferFilter() = default;
  //set circular buffer capacity.
  CircularBufferFilter* set_capacity(int capacity) {
    circular_buffer_.set_capacity(capacity);
    return this;
  }
  bool Init(MediaContext* bus) override;
  bool Filter(MediaContext* ctx, const DataType &data) override;
  void Notify(MediaContext *ctx, const MediaNotify &notify) override;
  void Close(MediaContext* ctx) override;

private:
  void UnqueueBuffer(MediaContext *ctx);

private:
  std::atomic_bool to_release_ = {false};
  std::atomic_bool err_ = {false};
  dynamic_circular_buffer<DataType> circular_buffer_;
  std::thread run_thread_;
  std::mutex mtx_;
  std::condition_variable unqueue_condition_;
};

template<typename DataType>
bool CircularBufferFilter<DataType>::Init(MediaContext* ctx) {
  CHECK(this->next_filter_ != nullptr);
  if (!this->next_filter_->Init(ctx)) {
    return false;
  } else {
    to_release_ = false;
    err_ = false;
    run_thread_ = std::thread(&CircularBufferFilter<DataType>::UnqueueBuffer, this, ctx);
    return true;
  }
}

template<typename DataType>
bool CircularBufferFilter<DataType>::Filter(MediaContext* ctx, const DataType &data) {
  if (err_) return false;
  std::unique_lock<std::mutex> lck(mtx_);
  circular_buffer_.push_back(data);
  unqueue_condition_.notify_one();
  return true;
}

template <typename DataType>
void CircularBufferFilter<DataType>::Notify(MediaContext *ctx, const MediaNotify &notify) {
  if (notify.type == kNotifyEOF) {
//    LOG(VERBOSE) << "CircularBufferFilter flush buffer.";
//    std::unique_lock<std::mutex> lck(mtx);
//    queue_condition_.wait(lck, [this]() {
//      return circular_buffer_.empty() || err_;
//    });
  }
  this->next_filter_->Notify(ctx, notify);
}

template<typename DataType>
void CircularBufferFilter<DataType>::Close(MediaContext *ctx) {
  if (!to_release_) {
    to_release_ = true;
    unqueue_condition_.notify_all();
    if (run_thread_.joinable()) {
      run_thread_.join();
    }
    LOG(VERBOSE) << "CircularBuffer close.";
    this->next_filter_->Close(ctx);
  }
}

template<typename DataType>
void CircularBufferFilter<DataType>::UnqueueBuffer(MediaContext *ctx) {
  while (!to_release_ || !err_) {
    std::unique_lock<std::mutex> lck(mtx_);
    unqueue_condition_.wait(lck, [this]() {
      return !circular_buffer_.empty() || to_release_;
    });
    if (to_release_) break;
    auto &data = circular_buffer_.front();

    //LOG(INFO) << "to filter:" << buffer_queue_.size();
    auto ret = this->next_filter_->Filter(ctx, data);
    if (!ret) err_ = true;
    circular_buffer_.pop_front();
  }
  LOG(VERBOSE) << "CircularBufferFilter exit thread";
}


}



