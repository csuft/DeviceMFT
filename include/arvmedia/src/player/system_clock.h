//
//  SystemClock.h
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/20.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef system_clock_h
#define system_clock_h

#include "media_clock.h"
#include "llog/llog.h"
#include <chrono>
#include <mutex>

namespace ins {
  class SystemClock : public MediaClock {
  public:
    int64_t NowInMs() const noexcept override {
      std::lock_guard<std::mutex> lk(mtx_);
      if (status_ == kNotStart) {
        return -1;
      } else if (status_ == kPaused) {
        auto pass = pause_point_ - start_point_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(pass).count();
      } else {
        auto pass = std::chrono::steady_clock::now() - start_point_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(pass).count();
      }
    }
    
    void Start(int64_t start_clock) noexcept override {
      CHECK(status_ == kNotStart) << "must Start on status NotStart";
      std::lock_guard<std::mutex> lk(mtx_);
      status_ = kStarted;
      start_point_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(start_clock);
      LOG(VERBOSE) << "SystemClock Start";
    }
    
    void Pause() noexcept override {
      CHECK(status_ == kStarted) << "must Pause on status Started";
      std::lock_guard<std::mutex> lk(mtx_);
      status_ = kPaused;
      pause_point_ = std::chrono::steady_clock::now();
      LOG(VERBOSE) << "SystemClock Pause";
    }

    void ForceUpdateClock(int64_t clock) noexcept override {
      std::lock_guard<std::mutex> lk(mtx_);

      switch (status_) {
        case kStarted: {
          start_point_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(clock);
          break;
        }

        case kPaused: {
          start_point_ = pause_point_ - std::chrono::milliseconds(clock);
          break;
        }

        default: {
          break;
        }
      }
      LOG(VERBOSE) << "SystemClock ForceUpdateClock to " << clock;
    }

    void Resume() noexcept override {
      CHECK(status_ == kPaused) << "must Resume on status Paused";
      std::lock_guard<std::mutex> lk(mtx_);
      status_ = kStarted;
      start_point_ += (std::chrono::steady_clock::now()-pause_point_);
      LOG(VERBOSE) << "SystemClock resume";
    }

    void Reset() noexcept override {
      status_ = kNotStart;
      LOG(VERBOSE) << "SystemClock reset";
    }
    
  private:
    std::chrono::steady_clock::time_point start_point_;
    std::chrono::steady_clock::time_point pause_point_;
    mutable std::mutex mtx_;
//    std::chrono::milliseconds paused_duration_ = std::chrono::milliseconds::zero();
  };
}

#endif /* SystemClock_h */
