//
//  media_clock.h
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/20.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef media_clock_h
#define media_clock_h

#include <stdint.h>

namespace ins {

class MediaClock {
public:
  virtual ~MediaClock() {}
  virtual int64_t NowInMs() const = 0; // return -1 when invalid
  virtual void Start(int64_t startTimeUs) = 0;
  virtual void Pause() = 0;
  virtual void ForceUpdateClock(int64_t clock) = 0;
  virtual void Resume() = 0;
  virtual void Reset() = 0;

protected:
  enum Status {
    kNotStart,
    kStarted,
    kPaused,
  };
  Status status_ = kNotStart;
};

}


#endif /* media_clock_h */
