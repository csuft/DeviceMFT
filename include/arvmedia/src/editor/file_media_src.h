//
//  file_media_src.h
//
//  Created by jerett on 16/7/13.
//  Copyright © 2016 Insta360. All rights reserved.
//
#pragma once

#include <string>
#include <tuple>
#include <climits>
#include <atomic>
#include <chrono>
#include "media_src.h"
#include "av_toolbox/sp.h"
#include "eventloop/event_handler.h"

struct AVStream;

namespace ins {

class any;
class Demuxer;
class EventDispatcher;

class FileMediaSrc : public MediaSrc, public EventHandler, public std::enable_shared_from_this<FileMediaSrc> {
public:
  explicit FileMediaSrc(const std::string &url);
  ~FileMediaSrc();
  FileMediaSrc(const FileMediaSrc&) = delete;
  FileMediaSrc(FileMediaSrc&&) = delete;
  FileMediaSrc& operator=(const FileMediaSrc&) = delete;

  /**
   * @param start_ms Start time in ms
   * @param end_ms   End time in ms
   */
  void set_trim_range(int64_t start_ms, int64_t end_ms) noexcept;

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

  const AVStream* video_stream() const;
  const AVStream* audio_stream() const;
  int64_t video_duration() const;
  int64_t audio_duration() const;

  FileMediaSrc* set_realtime(bool realtime) {
    realtime_ = realtime;
    return this;
  }

private:
  double progress() const override {
    return progress_;
  }

  bool Prepare() override;
  void Start(int64_t offset_ms = 0) override;
  void Cancel() override;
  void Close() override;
  void NotifyFirstPktDts();

private:
  void HandleEvent(const sp<Event> &event) override;
  void PerformStart();
  void PerformLoop();
  void PerformClose(const sp<Event> &event);
  void PerformOnCanceled();
  void PerformOnEnd();
  void PerformOnError();

  bool NeedLoopVideo() const;
  bool NeedLoopAudio() const;

private:
  bool eof_ = false;
  double progress_ = 0;
  std::atomic_bool canceled_ = {false};
  std::atomic_bool on_loop_ = {false};
  bool realtime_ = false;
  struct LoopData {
    int64_t video_start_dts = INT_MIN;
    int64_t video_offset_dts_ = 0;
    int64_t last_video_dts_ = 0;

    int64_t audio_start_dts = INT_MIN;
    int64_t audio_offset_dts_ = 0;
    int64_t last_audio_dts_ = 0;
    int64_t duration_ms;
    std::chrono::steady_clock::time_point start_clock;
  } loop_data_;
  std::string file_url_;
  std::tuple<bool, int64_t, int64_t> range_ = { false, 0, 0 };
  MediaContext video_ctx_;
  MediaContext audio_ctx_;
  sp<Demuxer> demuxer_;
  sp<PacketSource> video_filter_ = nullptr;
  sp<PacketSource> audio_filter_ = nullptr;

  sp<EventDispatcher> dispatcher_;
};

}
