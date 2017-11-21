//
// Created by jerett on 16/6/15.
// Copyright (c) 2016 Insta360. All rights reserved.
//

#include "openal_audio_player.h"

#if !(TARGET_OS_IPHONE)
extern "C" {
#include <libavformat/avformat.h>
}
#include <llog/llog.h>

namespace ins {

void CloseALCDevice(ALCdevice *device) {
  if (device) {
    LOG(VERBOSE) << "Close Audio Device ...";
    alcCloseDevice(device);
  }
}

void CloseALCContext(ALCcontext *context) {
  if (context) {
    LOG(VERBOSE) << "Close context ....";
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
  }
}

void DeleteALSource(ALuint *source) {
  if (source) {
    LOG(VERBOSE) << "delete source ...";
    alDeleteSources(1, source);
    delete source;
  }
}

void DeleteALBuffer(ALuint *buffer) {
  if (buffer) {
    LOG(VERBOSE) << "delete buffer ...";
    alDeleteBuffers(kBufferNum, buffer);
//    delete buffer;
    delete []buffer;
  }
}

OpenALAudioPlayer::OpenALAudioPlayer(int sample_rate,
                                     int channel,
                                     const AudioDataSourceCallback &pcm_data_func) noexcept
  :super(sample_rate, channel, pcm_data_func),
  device_(nullptr, nullptr),
  context_(nullptr, nullptr),
  source_(nullptr, nullptr),
  buffer_(new ALuint[kBufferNum], DeleteALBuffer) {
  auto ret = alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT");
  CHECK(ret == AL_TRUE) << "ALC_ENUMERATION_EXT not exists.";

  auto device_name = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
  LOG(VERBOSE) << "play audio device name:" << device_name;

  auto device = alcOpenDevice(device_name);
  device_ = decltype(device_)(device, CloseALCDevice);

  auto context = alcCreateContext(device_.get(), nullptr);
  if (context == nullptr) {
    LOG(ERROR) << "Create openal context error !";
  }
  context_ = decltype(context_)(context, CloseALCContext);
  alcMakeContextCurrent(context_.get());

  source_ = decltype(source_)(new ALuint, DeleteALSource);
  alGenSources(1, source_.get());
  alSourcef(*source_, AL_GAIN, 1);
  alSource3f(*source_, AL_POSITION, 0, 0, 0);

  alGenBuffers(kBufferNum, buffer_.get());
}

void OpenALAudioPlayer::FillSlience(ALuint *bufferID) {
  static char slience[4096] = {0};
  alGetError();
  alBufferData(*bufferID, AL_FORMAT_STEREO16, slience, 4096, sample_rate_);
  auto ret = alGetError();
  if (ret != AL_NO_ERROR) {
    LOG(ERROR) << "error ";
  }
}

void OpenALAudioPlayer::StartPlay() {
  {
    CHECK(kNotStart == play_status_) << "You Must StartPlay when on status:kNotStart";
    play_status_ = kStarted;
    // lck will unlock. otherwise will lead to dead lock
  }

  for (int i = 0; i < kBufferNum; ++i) {
    FillSlience(&buffer_[i]);
    alSourceQueueBuffers(*source_, 1, &buffer_[i]);
  }
//  alSourcePlay(*source_);
  audio_thread_ = std::thread(&OpenALAudioPlayer::PlayAudioThread, this);
  LOG(VERBOSE) << "OpenALAudioPLayer StartPlay.";
}

void OpenALAudioPlayer::PausePlay() {
  TimedBlock(pause_obj, "opeanl pause play.");
  {
    CHECK(kStarted == play_status_) << "You Must PausePlay when on status:kStarted";
    play_status_ = kPaused;
    // lck will unlock. otherwise will lead to dead lock
  }

  TimedBlock(obj, "opeanl pause join thread.");
  if (audio_thread_.joinable()) audio_thread_.join();
  LOG(VERBOSE) << "OpenALAudioPLayer PausePlay.";
}

void OpenALAudioPlayer::ResumePlay() {
  {
    CHECK(kPaused == play_status_) << "You Must ResumePlay when on status:kPaused";
    play_status_ = kStarted;
    // lck will unlock. otherwise will lead to dead lock
  }

  audio_thread_ = std::thread(&OpenALAudioPlayer::PlayAudioThread, this);
  LOG(VERBOSE) << "OpenALAudioPLayer ResumePlay.";
}

void OpenALAudioPlayer::PlayAudioThread() {
  alSourcePlay(*source_);//must play first. otherwise audio thread will buffer data error
  while (play_status_ ==  kStarted) {
    ALint buffer_processed = 0;
    alGetSourcei(*source_, AL_BUFFERS_PROCESSED, &buffer_processed);
    auto error = alGetError();
    if (error != AL_NO_ERROR) {
      LOG(ERROR) << "get buffer processed:" << error;
    }

    for (int i = 0; i < buffer_processed; ++i) {
      ALuint buffer_id;
      alSourceUnqueueBuffers(*source_, 1, &buffer_id);
      error = alGetError();
      if (error != AL_NO_ERROR) {
        LOG(ERROR) << "unqueue buffer error:" << error;
      }

      auto pcm_frame = pcm_data_func_();
      if (pcm_frame) {
//        LOG(INFO) << "play pcm :" << pcm_data->pts()
//              << " size:" << pcm_data->size()
//              << " sample_rate:" << sample_rate_;
        current_position_ = pcm_frame->pts;
        alBufferData(buffer_id, AL_FORMAT_STEREO16, pcm_frame->data[0], pcm_frame->linesize[0], pcm_frame->sample_rate);
        error = alGetError();
        if (error != AL_NO_ERROR) {
          LOG(ERROR) << "buffer data error:" << alGetError();
        }
      } else {
        FillSlience(&buffer_id);
//        LOG(INFO) << "to fill slience....";
      }
      alSourceQueueBuffers(*source_, 1, &buffer_id);
      error = alGetError();
      if (error != AL_NO_ERROR) {
        LOG(ERROR) << "queue buffer error:" << error;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  ALint buffer_processed = 0;
  while (buffer_processed != kBufferNum) {
    alGetSourcei(*source_, AL_BUFFERS_PROCESSED, &buffer_processed);
  }
  alSourcePause(*source_); // should pause after thread join. otherwise thread will release
  LOG(VERBOSE) << "exit audio thread with" << buffer_processed << " prrocessed";
}

OpenALAudioPlayer::~OpenALAudioPlayer() {
  if (play_status_ == kStarted) {
    PausePlay();
  }
  LOG(VERBOSE) << "~OpenALAudioPlayer";
}

}

#endif
