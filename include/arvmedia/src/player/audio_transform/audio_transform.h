//
// Created by jerett on 2017/6/14.
//

#pragma once

#include <av_toolbox/sp.h>

struct AVStream;
struct AVCodecParameters;
struct AVFrame;

namespace ins {

struct AudioTransformContext {
  bool spatial_audio = false;
  const AVStream *in_stream = nullptr;
  sp<AVCodecParameters> out_codecpar;
};

class AudioTransform {
public:
  AudioTransform() noexcept = default;
  virtual ~AudioTransform() = default;
  AudioTransform(const AudioTransform&) = delete;
  AudioTransform(AudioTransform&&) = delete;
  AudioTransform& operator=(const AudioTransform&) = delete;

  virtual bool Init(AudioTransformContext *ctx) = 0;
  virtual bool Transform(sp<AVFrame> in, sp<AVFrame> &out) = 0;

  void set_next_transform(const sp<AudioTransform> &next_transform) {
    next_transform_ = next_transform;
  }

  sp<AudioTransform> next_transform() const noexcept {
    return next_transform_;
  }

protected:
  sp<AudioTransform> next_transform_;

};

}

