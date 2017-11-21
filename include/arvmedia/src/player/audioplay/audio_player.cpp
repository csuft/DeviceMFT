//
// Created by jerett on 16/12/18.
//

#include "audio_player.h"
#include "openal_audio_player.h"
#include "audio_queue_player.h"
#include <player/system_clock.h>
#include <llog/llog.h>

namespace ins {

up<AudioPlayer> SuitableAudioPlayer(int sample_rate, int channel, const AudioDataSourceCallback &pcm_data_func) {
  #if __APPLE__
  LOG(INFO) << "using audio queue player.";
  up<AudioPlayer> audio_player(new AudioQueuePlayer(sample_rate, channel, pcm_data_func));
  return audio_player;
  #else
  LOG(INFO) << "using OpenAL player.";
  up<AudioPlayer> audio_player(new OpenALAudioPlayer(sample_rate, channel, pcm_data_func));
  return audio_player;
  #endif
}

int64_t AudioPlayer::NowInMs() const {
  if (sys_clock_) {
    return sys_clock_->NowInMs();
  } else {
    return current_position_;
  }
}

void AudioPlayer::Start(int64_t start_ms) {
  CHECK(kStarted == play_status_) << "You Must StartPlay first when want to Pause clock";
  CHECK(kNotStart == status_) << "You Must start clock when Status Not started";

  if (sys_clock_) sys_clock_->Start(start_ms);
  status_ = kStarted;
  current_position_ = start_ms;
  LOG(VERBOSE) << "AudioPlayer as Clock start.....";
}

void AudioPlayer::ForceUpdateClock(int64_t clock) {
  if (sys_clock_) sys_clock_->ForceUpdateClock(clock);
  current_position_ = clock;
  LOG(VERBOSE) << "AudioPlayer ForceUpdateClock to " << clock;
}

void AudioPlayer::Pause() {
  if (sys_clock_) sys_clock_->Pause();
  CHECK(kPaused == play_status_) << "You Must PausePlay first when want to Pause clock";
  CHECK(kStarted == status_) << "You Must Pause when on status:kStarted";

  status_ = kPaused;
  LOG(VERBOSE) << "AudioPLayer as Clock Paused.....";
}

void AudioPlayer::Resume() {
  if (sys_clock_) sys_clock_->Resume();
  CHECK(kStarted == play_status_) << "You Must StartPlay first when want to Resume clock";
  CHECK(kPaused == status_) << "You Must Resume when on status:kPaused";

  status_ = kStarted;
  LOG(VERBOSE) << "AudioPLayer as Clock Resume";
}

void AudioPlayer::Reset() {
  if (sys_clock_) sys_clock_->Reset();
  status_ = kNotStart;
  LOG(VERBOSE) << "AudioPlayer as Clock Reset";
}

void AudioPlayer::ChangeToSystemClock() {
  sys_clock_ = std::make_shared<SystemClock>();
  sys_clock_->Start(current_position_);
  if (status_ == kPaused) {
    sys_clock_->Pause();
  }
  LOG(VERBOSE) << "change from audio clock to system clock :" << current_position_ << "ms";
}

void AudioPlayer::ChnageToAudioClock() {
  if (sys_clock_) {
    sys_clock_.reset();
    LOG(VERBOSE) << "change from sys clock to audio clock";
  }
}


}

