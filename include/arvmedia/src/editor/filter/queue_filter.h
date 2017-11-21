//
// Created by jerett on 16/7/24.
//

#ifndef INSPLAYER_QUEUE_FILTER_H
#define INSPLAYER_QUEUE_FILTER_H

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "media_filter.h"

namespace ins {

const static std::string kQueueFilterMaxSize("queue_size"); //value type:int, default 10

template <typename DataType>
class QueueFilter : public MediaFilter<DataType, DataType> {
public:
  QueueFilter();
  ~QueueFilter();

  QueueFilter* set_max_size(int max_size) {
    max_size_ = max_size;
    return this;
  }

  bool Init(MediaContext *ctx) override;
  bool Filter(MediaContext *ctx, const DataType &data) override;
  void Notify(MediaContext *ctx, const MediaNotify &notify) override;
  void Close(MediaContext *ctx) override;

private:
  void UnqueueBuffer(MediaContext *ctx);

private:
  /// set default queue max size 10
  int max_size_ = 10;
  std::atomic_bool to_release_ = {false};

  std::mutex queue_mtx_;
  std::atomic_bool err_ = {false};
  std::condition_variable unqueue_condition_;
  std::condition_variable queue_condition_;

  std::thread unqueue_thread_;
  std::queue<DataType> buffer_queue_;
};

template <typename DataType>
QueueFilter<DataType>::QueueFilter() = default;

template <typename DataType>
QueueFilter<DataType>::~QueueFilter() = default;

template <typename DataType>
bool QueueFilter<DataType>::Init(MediaContext *ctx) {
  CHECK(this->next_filter_ != nullptr);
  if (!this->next_filter_->Init(ctx)) {
    return false;
  } else {
    to_release_ = false;
    err_ = false;
    unqueue_thread_ = std::thread(&QueueFilter::UnqueueBuffer, this, ctx);
    return true;
  }
}

template <typename DataType>
bool QueueFilter<DataType>::Filter(MediaContext *ctx, const DataType &data) {
  if (err_) return false;
  std::unique_lock<std::mutex> lck(queue_mtx_);
  if (max_size_ > 0) {
    queue_condition_.wait(lck, [this]() {
                        return buffer_queue_.size() < max_size_;
                      });
  }
  buffer_queue_.push(data);
  unqueue_condition_.notify_one();
  return true;
}

template <typename DataType>
void QueueFilter<DataType>::Notify(MediaContext *ctx, const MediaNotify &notify) {
  if (notify.type == kNotifyEOF) {
    LOG(VERBOSE) << "QueueFilter flush buffer.";
    std::unique_lock<std::mutex> lck(queue_mtx_);
    queue_condition_.wait(lck, [this]() {
      return buffer_queue_.empty() || err_;
    });
  }
  this->next_filter_->Notify(ctx, notify);
}

template <typename DataType>
void QueueFilter<DataType>::Close(MediaContext *ctx) {
  if (!to_release_) {
    to_release_ = true;
    unqueue_condition_.notify_all();
    if (unqueue_thread_.joinable()) {
      unqueue_thread_.join();
    }
    LOG(VERBOSE) << "QueueFilter Close.";
    this->next_filter_->Close(ctx);
  }
}

template <typename T>
void QueueFilter<T>::UnqueueBuffer(MediaContext *ctx) {
  while (!to_release_ || !err_) {
    std::unique_lock<std::mutex> lck(queue_mtx_);
    unqueue_condition_.wait(lck, [this]() {
                          return !buffer_queue_.empty() || to_release_;
                        });
    if (to_release_) break;
    auto &data = buffer_queue_.front();
    //          LOG(INFO) << "to filter:" << buffer_queue_.size();
    auto ret = this->next_filter_->Filter(ctx, data);
    if (!ret) err_ = true;
    buffer_queue_.pop();
    queue_condition_.notify_one();
  }
  LOG(VERBOSE) << "QueueFilter exit thread.";
}


}


#endif //INSPLAYER_QUEUE_FILTER_H
