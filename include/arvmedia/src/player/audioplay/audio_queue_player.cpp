//
//  audio_player.cpp
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/22.
//  Copyright © 2016 insta360. All rights reserved.
//

#include "audio_queue_player.h"

extern "C" {
#include <libavformat/avformat.h>
}

#if __APPLE__
#include <llog/llog.h>

namespace ins {
void HandleOutputBuffer(void *aqData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
  AudioQueuePlayer *player = reinterpret_cast<AudioQueuePlayer *>(aqData);
  player->FillAudioBuffer(inBuffer);
}

AudioQueuePlayer::AudioQueuePlayer(int sample_rate,
                                   int channel,
                                   const AudioDataSourceCallback &pcm_data_func)
    : super(sample_rate, channel, pcm_data_func){
  CHECK(pcm_data_func != nullptr) << "pcm func must not be nullptr!";
  AudioStreamBasicDescription streamFormat = {0};
  streamFormat.mFormatID = kAudioFormatLinearPCM;
  streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  streamFormat.mSampleRate = sample_rate;
  streamFormat.mFramesPerPacket = 1;
  streamFormat.mReserved = 0;
  streamFormat.mBytesPerPacket = 4;
  streamFormat.mBytesPerFrame = 4;
  streamFormat.mChannelsPerFrame = 2;
  streamFormat.mBitsPerChannel = 16;

  // New input queue
  OSStatus err = AudioQueueNewOutput(&streamFormat, HandleOutputBuffer, this, nullptr, kCFRunLoopCommonModes, 0, &audio_queue_);
  if (err != noErr) {
    LOG(INFO) << "AudioQueueNewOutput err";
    return;
  }

  int vBufferSize = 4096;
  for (int i = 0; i < kAudioBufferNum; ++i) {
    if ((err = AudioQueueAllocateBuffer(audio_queue_, vBufferSize, &audio_queue_buffers_[i]))!=noErr) {
      LOG(ERROR) << "Error: Could not allocate audio queue buffer:" << err;
      AudioQueueDispose(audio_queue_, true);
      return;
    }
  }

  Float32 gain=1.0;
  AudioQueueSetParameter(audio_queue_, kAudioQueueParam_Volume, gain);
}

AudioQueuePlayer::~AudioQueuePlayer() {
  AudioQueueDispose(audio_queue_, true);
  LOG(VERBOSE) << "~AudioQueuePlayer";
}

void AudioQueuePlayer::StartPlay() {
  CHECK(kNotStart == play_status_) << "You Must StartPlay when on status:kNotStart";
  for (int i = 0; i < kAudioBufferNum; ++i) {
    FillAudioBuffer(audio_queue_buffers_[i]);
  }

  OSStatus err = AudioQueueStart(audio_queue_, nullptr);
  if (err != noErr) {
    LOG(ERROR) << "AudioQueueStart() error:" << err;
  }

  play_status_ = kStarted;
  LOG(VERBOSE) << "AudioQueuePlayer StartPlay";
}

void AudioQueuePlayer::PausePlay() {
  CHECK(kStarted == play_status_) << "You Must Pause when on status:kStarted";
  AudioQueuePause(audio_queue_);
  play_status_ = kPaused;
  LOG(VERBOSE) << "AudioQueuePlayer Pause";
}

void AudioQueuePlayer::ResumePlay() {
  CHECK(kPaused == play_status_) << "You Must Resume on status:kPaused";
  AudioQueueStart(audio_queue_, nullptr);
  play_status_ = kStarted;
  LOG(VERBOSE) << "AudioQueuePlayer Resume";
}

void AudioQueuePlayer::FillBufferWithSlience(AudioQueueBufferRef buffer) {
  static char slienceData[4096] = {0};
  memcpy(buffer->mAudioData, slienceData, sizeof(slienceData));
  buffer->mAudioDataByteSize = sizeof(slienceData);
  OSStatus err = AudioQueueEnqueueBuffer(audio_queue_, buffer, 0 , NULL);
  if (err != noErr) {
//      LOG(WARNING) << "fill slience err" << err;
  }
}

void AudioQueuePlayer::FillAudioBuffer(AudioQueueBufferRef buffer){
  auto pcm_frame = pcm_data_func_();

  if (pcm_frame) {
    current_position_ = pcm_frame->pts;
    memcpy(buffer->mAudioData, pcm_frame->data[0], pcm_frame->linesize[0]);
    buffer->mAudioDataByteSize = pcm_frame->linesize[0];
    OSStatus err = AudioQueueEnqueueBuffer(audio_queue_, buffer, 0 , NULL);

    if (err != noErr) {
//        LOG(ERROR) << "AudioQueueEnqueueBuffer() error:" << int(err);
    }
  } else {
    FillBufferWithSlience(buffer);
//        LOG(WARNING) << "no data to fill audio buffer";
  }
}
}

#endif