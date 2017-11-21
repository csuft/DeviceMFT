//
// Created by jerett on 16/8/29.
//

#pragma once

#include <functional>
#include "filter/media_filter.h"
#include "av_toolbox/any.h"

namespace ins {

class MediaSrc {
  friend class MediaPipe;
  friend class MergeMediaSrc;
public:
  enum MediaSrcState {
    MediaSourceEnd = 0,
    MediaSourceCanceled,
    MediaSourceError,
  };
  using STATE_CALLBACK = std::function<void(MediaSrcState state, const any &extra)>;
  MediaSrc() = default;
  virtual ~MediaSrc() = default;

  /// register state callback
  /// \param state_callback
  void RegisterCallback(const STATE_CALLBACK &state_callback) noexcept {
    state_callback_ = state_callback;
  }
  
private:
  virtual double progress() const = 0;
  virtual bool Prepare() = 0;
  virtual void Start(int64_t timestamp_offset_ms = 0) = 0;
  virtual void Cancel() = 0;
  virtual void Close() = 0;

protected:
  STATE_CALLBACK state_callback_;
  /**
   * \brief Set by media pipe.
   */
  int index_ = -1;
  int error_code_ = -1;
};

}

