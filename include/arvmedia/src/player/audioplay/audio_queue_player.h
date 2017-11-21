//
//  audio_player.hpp
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/22.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef audio_queue_player_h
#define audio_queue_player_h

#include "av_toolbox/platform.h"
#if (__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <functional>
#include <mutex>
#include "audio_player.h"

namespace ins {
class MediaData;

class AudioQueuePlayer: public AudioPlayer{
  friend void HandleOutputBuffer(void *aqData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer);
  using super = AudioPlayer;
public:

  AudioQueuePlayer(int sample_rate,
                   int channel,
                   const AudioDataSourceCallback &pcm_data_func);

  ~AudioQueuePlayer();

  void StartPlay() override;
  void PausePlay() override;
  void ResumePlay() override;

private:
  void FillBufferWithSlience(AudioQueueBufferRef buffer);
  void FillAudioBuffer(AudioQueueBufferRef buffer);

private:
  const static int kAudioBufferNum = 10;
  AudioQueueRef audio_queue_;
  AudioQueueBufferRef audio_queue_buffers_[kAudioBufferNum];
};
}
#endif

#endif /* audio_player_h */
