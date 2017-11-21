#pragma once

#include "media_src.h"
#include "filter/media_filter.h"

extern "C" {
#include <libavformat/avformat.h>
}
#include <av_toolbox/ffmpeg_util.h>

struct AVFrame;
struct AVStream;

namespace ins {

template <typename VideoSourceType, typename AudioSourceType>
class RealtimeMediaSrc {
public:
  RealtimeMediaSrc() noexcept = default;
  
  template <typename FilterType>
  FilterType& set_video_filter(const FilterType &filter) noexcept {
    video_filter_ = filter;
    return const_cast<FilterType&>(filter);
  }

  template <typename FilterType>
  FilterType& set_audio_filter(const FilterType &filter) noexcept {
    audio_filter_ = filter;
    return const_cast<FilterType&>(filter);
  }

  void AddVideoStream(const sp<AVStream> &video_stream);
  bool SendVideo(const VideoSourceType &frame);

  void AddAudioStream(const sp<AVStream> &audio_stream);
  bool SendAudio(const AudioSourceType &pkt);

  bool Prepare();
  void Stop();

private:
  MediaContext video_ctx_;
  MediaContext audio_ctx_;
  sp<AVStream> video_stream_;
  sp<AVStream> audio_stream_;
  sp<MediaSource<VideoSourceType>> video_filter_;
  sp<MediaSource<AudioSourceType>> audio_filter_;
};

template<typename VideoSourceType, typename AudioSourceType>
inline void RealtimeMediaSrc<VideoSourceType, AudioSourceType>::AddVideoStream(const sp<AVStream>& video_stream) {
  video_stream_ = video_stream;
  video_stream_->index = 0;
}

template<typename VideoSourceType, typename AudioSourceType>
inline bool RealtimeMediaSrc<VideoSourceType, AudioSourceType>::SendVideo(const VideoSourceType& frame) {
  return video_filter_->Filter(&video_ctx_, frame);
}

template<typename VideoSourceType, typename AudioSourceType>
inline void RealtimeMediaSrc<VideoSourceType, AudioSourceType>::AddAudioStream(const sp<AVStream>& audio_stream) {
    audio_stream_ = audio_stream;
    audio_stream_->index = 1;
}

template<typename VideoSourceType, typename AudioSourceType>
inline bool RealtimeMediaSrc<VideoSourceType, AudioSourceType>::SendAudio(const AudioSourceType& pkt) {
  return audio_filter_->Filter(&audio_ctx_, pkt);
}

template<typename VideoSourceType, typename AudioSourceType>
inline bool RealtimeMediaSrc<VideoSourceType, AudioSourceType>::Prepare() {
  if (video_filter_) {
    video_ctx_.input_stream.stream = video_stream_.get();
    auto codecpar = NewAVCodecParameters();
    avcodec_parameters_copy(codecpar.get(), video_stream_->codecpar);
    video_ctx_.output_stream.codecpar = codecpar;
    if (!video_filter_->Init(&video_ctx_)) return false;
  }

  if (audio_filter_) {
    audio_ctx_.input_stream.stream = audio_stream_.get();
    auto codecpar = NewAVCodecParameters();
    avcodec_parameters_copy(codecpar.get(), audio_stream_->codecpar);
    audio_ctx_.output_stream.codecpar = codecpar;
    if (!audio_filter_->Init(&audio_ctx_)) return false;
  }
  return true;
}

template<typename VideoSourceType, typename AudioSourceType>
inline void RealtimeMediaSrc<VideoSourceType, AudioSourceType>::Stop() {
  if (video_filter_) {
    video_filter_->Close(&video_ctx_);
  }
  if (audio_filter_) {
    audio_filter_->Close(&audio_ctx_);
  }
}

}
