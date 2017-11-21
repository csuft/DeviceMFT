//
// Created by jerett on 16/6/15.
// Copyright (c) 2016 Insta360. All rights reserved.
//

#ifndef INSMEDIAAPP_AUDIO_PLAYER_H
#define INSMEDIAAPP_AUDIO_PLAYER_H

#include <functional>
#include <memory>
#include "player/media_clock.h"
#include "av_toolbox/sp.h"

struct AVFrame;

namespace ins {

class AudioPlayer;
class SystemClock;
using AudioDataSourceCallback = std::function<sp<AVFrame>()>;

up<AudioPlayer> SuitableAudioPlayer(int sample_rate, int channel, const AudioDataSourceCallback &pcm_data_func);

class AudioPlayer : public MediaClock {
public:
  virtual ~AudioPlayer() {}

  //media clock
  int64_t NowInMs() const override; // return -1 when invalid
  void Start(int64_t start_ms) override;
  void ForceUpdateClock(int64_t clock) override;
  void Pause() override;
  void Resume() override;
  void Reset() override;
  
  void ChangeToSystemClock();
  void ChnageToAudioClock();

  //audio player
  virtual void StartPlay() = 0;
  virtual void PausePlay() = 0;
  virtual void ResumePlay() = 0;

protected:
  AudioPlayer(int sample_rate, int channel, const AudioDataSourceCallback &pcm_data_func)
      : sample_rate_(sample_rate), channel_(channel), pcm_data_func_(pcm_data_func) {
  }

  Status play_status_ = kNotStart;
  int sample_rate_ = 0;
  int channel_ = 0;
  int64_t current_position_ = -1;
  AudioDataSourceCallback pcm_data_func_;

private:
  sp<SystemClock> sys_clock_;
};

}

#endif //INSMEDIAAPP_AUDIO_PLAYER_H
