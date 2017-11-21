//
// Created by jerett on 16/7/14.
//
#pragma once

#include <vector>
#include <numeric>
#include <thread>
#include "llog/llog.h"
#include "eventloop/event_handler.h"
#include "av_toolbox/sp.h"

namespace ins {

class EventDispatcher;
class MediaSrc;
class DynamicBitset;

class MediaPipe : public EventHandler, public std::enable_shared_from_this<MediaPipe> {
  enum MediaPipeInteralState {
    kMediaPipeUnknown,
    kMediaPipeStart,
    kMediaPipeReleased,
  };
public:
  enum MediaPipeState {
    kMediaPipeEnd = 100,
    kMediaPipeCanceled,
    kMediaPipeError
  };
  //using StateCallback = std::function<void(MediaPipeState state)>;
  using EndCallback = std::function<void()>;
  using CancelCallback = std::function<void()>;
  using ErrorCallback = std::function<void(int error_code)>;
  

public:
  MediaPipe() = default;
  ~MediaPipe() {
    CHECK(state_ == kMediaPipeReleased || state_ == kMediaPipeUnknown) 
      << "You must call Release before ~MediaPipe if you have called Start.";
    //Release();
    LOG(VERBOSE) << "~MediaPipe";
  }

  /**
   * \brief Add one media pipeline to the media pipe.
   * \param src Media source.
   */
  void AddMediaSrc(const sp<MediaSrc> &src);
  /**
   * \brief Register end callback. When pipe run end, will callback this function
   * \param end_callback 
   */
  void RegisterEndCallback(const EndCallback &end_callback) {
    end_callback_ = end_callback;
  }
  /**
   * \brief Register cancel callback. When pipe has been canceled, will callback this function
   * \param cancel_callback 
   */
  void RegisterCancelCallback(const CancelCallback &cancel_callback) {
    cancel_callback_ = cancel_callback;
  }
  /**
   * \brief Register error callback. When pipe occur error, will callback this function
   * \param error_callback 
   */
  void RegisterErrorCallback(const ErrorCallback &error_callback) {
    error_callback_ = error_callback;
  }
  /**
   * \brief Get the pipe progress.
   * \return Progress in [0.0f, 1.0f].
   */
  double progress() const;
  /**
   * \brief Prepare all pipeline.
   * \return Whether prepare success.
   */
  bool Prepare();
  /**
   * \brief Run the pipe.
   */
  void Run();
  /**
   * \brief Request the media pipe to cancel, will return immediately.
   */
  void Cancel();
  /**
   * \brief Release resource.
   */
  void Release();

private:
  void CloseSrc();
  void HandleEvent(const sp<Event> &event) override;
  void PerformStart();
  void PerformCancel();
  void PerformOnEnd(const sp<Event> &event);
  void PerformOnCanceled(const sp<Event> &event);
  void PerformOnError();
  void PerformRelease(const sp<Event> &event);

private:
  int state_ = kMediaPipeUnknown;
  bool on_err_ = false;

  sp<DynamicBitset> cancel_record_;
  sp<DynamicBitset> eof_record_;

  sp<EventDispatcher> dispatcher_;
  std::vector<sp<MediaSrc>> sources_;

  EndCallback end_callback_;
  CancelCallback cancel_callback_;
  ErrorCallback error_callback_;
};

}
