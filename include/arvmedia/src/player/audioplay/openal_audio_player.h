
//
// Created by jerett on 16/6/15.
// Copyright (c) 2016 Insta360. All rights reserved.
//

#ifndef INSMEDIAAPP_OPENAL_AUDIO_PLAYER_H
#define INSMEDIAAPP_OPENAL_AUDIO_PLAYER_H

#include "audio_player.h"
#include "av_toolbox/platform.h"
#if !(TARGET_OS_IPHONE) //don't compile this file on iphone
#if __APPLE__
#include <OpenAL/openal.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <thread>

namespace ins{

const static int kBufferNum = 5;
void CloseALCDevice(ALCdevice *device);
void CloseALCContext(ALCcontext *context);
void DeleteALSource(ALuint *source);
void DeleteALBuffer(ALuint *buffer);

class OpenALAudioPlayer: public AudioPlayer {
  using super = AudioPlayer;
public:
  OpenALAudioPlayer(int sample_rate,
                    int channel,
                    const AudioDataSourceCallback &pcm_data_func) noexcept;
  ~OpenALAudioPlayer();

  void StartPlay() override;
  void PausePlay() override;
  void ResumePlay() override;

private:
  void FillSlience(ALuint *bufferID);
  void PlayAudioThread();

private:
  std::unique_ptr<ALCdevice, decltype(CloseALCDevice)*> device_;
  std::unique_ptr<ALCcontext, decltype(CloseALCContext)*> context_;
  std::unique_ptr<ALuint, decltype(DeleteALSource)*> source_;
  std::unique_ptr<ALuint[], decltype(DeleteALBuffer)*> buffer_;

  std::thread audio_thread_;
};

}

#endif
#endif //INSMEDIAAPP_OPENAL_AUDIO_PLAYER_H
