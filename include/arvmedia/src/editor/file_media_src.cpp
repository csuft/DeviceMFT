//
//  file_media_source.cpp
//  INSMediaApp
//
//  Created by jerett on 16/7/13.
//  Copyright © 2016 Insta360. All rights reserved.
//

#include "file_media_src.h"
extern "C" {
#include <libavformat/avformat.h>
}
#include <chrono>
#include <algorithm>
#include <future>
#include "av_toolbox/demuxer.h"
#include "av_toolbox/ffmpeg_util.h"
#include "eventloop/event.h"
#include "eventloop/event_dispatcher.h"
#include "llog/llog.h"
#include "filter/media_filter.h"
#include "editor/editor_error.h"

namespace ins {

enum {
  kToStart,
  kToLoop,
  kToCancel,
  kToClose,
  kOnError,
  kOnEnd,
};

FileMediaSrc::FileMediaSrc(const std::string &url) : file_url_(url) {
  ;
}

FileMediaSrc::~FileMediaSrc() {
  LOG(VERBOSE) << "~FileMediaSrc:" << file_url_;
}

void FileMediaSrc::set_trim_range(int64_t start_ms, int64_t end_ms) noexcept {
  std::get<0>(range_) = true;
  std::get<1>(range_) = start_ms;
  std::get<2>(range_) = end_ms;
}

const AVStream* FileMediaSrc::video_stream() const {
  return demuxer_->video_stream();
}

const AVStream* FileMediaSrc::audio_stream() const {
  return demuxer_->audio_stream();
}

int64_t FileMediaSrc::video_duration() const {
  return demuxer_->video_duration();
}

int64_t FileMediaSrc::audio_duration() const {
  return demuxer_->audio_duration();
}

void FileMediaSrc::PerformOnEnd() {
  LOG(VERBOSE) << file_url_ << " on end.";
  eof_ = true;
  if (NeedLoopVideo()) {
    {
      MediaNotify notify;
      notify.type = kNotifyEOF;
      video_filter_->Notify(&video_ctx_, notify);
    }
  }
  if (NeedLoopAudio()) {
    MediaNotify notify;
    notify.type = kNotifyEOF;
    audio_filter_->Notify(&video_ctx_, notify);
  }

  if (state_callback_) {
    int64_t time_offset = -1;
    if (NeedLoopAudio()) {
      auto ast = demuxer_->audio_stream();
      auto audio_frame_duration = ast->codecpar->frame_size * 1000.0 / ast->codecpar->sample_rate;
      LOG(INFO) << "estimate audio frame duration:" << audio_frame_duration;
      time_offset = TimestampToMs(loop_data_.last_audio_dts_, ast->time_base) + audio_frame_duration;
    }
    if (NeedLoopVideo()) {
      auto vst = demuxer_->video_stream();
      auto video_frame_duration = vst->avg_frame_rate.den * 1000 / vst->avg_frame_rate.num;
      LOG(INFO) << "estimate video frame duration:" << video_frame_duration;
      auto video_time_offset = TimestampToMs(loop_data_.last_video_dts_, vst->time_base) + video_frame_duration;
      using namespace std;
      time_offset = max(time_offset, video_time_offset);
    }
    state_callback_(MediaSourceEnd, time_offset);
  }
}

void FileMediaSrc::PerformOnCanceled() {
  if (state_callback_) {
    state_callback_(MediaSourceCanceled, nullptr);
  }
}

void FileMediaSrc::PerformOnError() {
  video_ctx_.canceled = true;
  audio_ctx_.canceled = true;
  if (state_callback_) {
    state_callback_(MediaSourceError, nullptr);
  }
}

bool FileMediaSrc::NeedLoopVideo() const {
  return demuxer_->video_stream_index() >= 0 && video_filter_;
}

bool FileMediaSrc::NeedLoopAudio() const {
  return demuxer_->audio_stream_index() >= 0 && audio_filter_;
}

void FileMediaSrc::NotifyFirstPktDts() {
  if (NeedLoopVideo()) {
    MediaNotify notify;
    notify.type = kNotifyFirstVideoPktDts;
    notify.user_data = loop_data_.video_start_dts;
    video_filter_->Notify(&video_ctx_, notify);
  }

  if (NeedLoopAudio()) {
    MediaNotify notify;
    notify.type = kNotifyFirstAudioPktDts;
    notify.user_data = loop_data_.audio_start_dts;
    audio_filter_->Notify(&audio_ctx_, notify);
  }
}

//bool FileMediaSrc::ReadTestPkt(sp<ARVPacket> &video_pkt, sp<ARVPacket> &audio_pkt) {
//  auto need_video_pkt = demuxer_->video_stream_index() >= 0 && video_filter_;
//  auto need_audio_pkt = demuxer_->audio_stream_index() >= 0 && audio_filter_;
//  sp<ARVPacket> pkt;
//  while (true) {
//    auto ret = demuxer_->NextPacket(pkt);
//    if (ret == 0) {
//      if (pkt->media_type == AVMEDIA_TYPE_AUDIO) audio_pkt = pkt;
//      if (pkt->media_type == AVMEDIA_TYPE_VIDEO) video_pkt = pkt;
//      if (((need_video_pkt && video_pkt) || !need_video_pkt) &&
//          ((need_audio_pkt && audio_pkt) || !need_audio_pkt)) {
//        return true;
//      }
//    } else if (ret == AVERROR(EAGAIN)) {
//      ;
//    } else {
//      LOG(ERROR) << "no video pkt found in file:" << file_url_;
//      return false;
//    }
//  }
//}

bool FileMediaSrc::Prepare() {
  demuxer_.reset(new Demuxer(file_url_));
  auto ret = demuxer_->Open(nullptr);
  if (ret != 0) return false;
  canceled_ = false;
  eof_ = false;

  //init filter
  if (NeedLoopVideo()) 
  {
    auto video_stream = demuxer_->video_stream();
    auto codecpar = NewAVCodecParameters();
    avcodec_parameters_copy(codecpar.get(), video_stream->codecpar);

    video_ctx_.input_stream.stream = video_stream;
    video_ctx_.output_stream.codecpar = codecpar;
    video_ctx_.input_stream.index = index_;
    if (!video_filter_->Init(&video_ctx_)) return false;
  }

  if (NeedLoopAudio()) {
    auto audio_stream = demuxer_->audio_stream();
    auto codecpar = NewAVCodecParameters();
    avcodec_parameters_copy(codecpar.get(), audio_stream->codecpar);

    audio_ctx_.input_stream.stream = audio_stream;
    audio_ctx_.output_stream.codecpar = codecpar;
    audio_ctx_.input_stream.index = index_;
    if (!audio_filter_->Init(&audio_ctx_)) {
      if (NeedLoopVideo()) video_filter_->Close(&video_ctx_);
      return false;
    }
  }

  return true;
}

void FileMediaSrc::Start(int64_t offset_ms) {
  if (NeedLoopVideo()) {
    loop_data_.video_offset_dts_ = MsToTimestamp(offset_ms, demuxer_->video_stream()->time_base);
  }
  if (NeedLoopAudio()) {
    loop_data_.audio_offset_dts_ = MsToTimestamp(offset_ms, demuxer_->audio_stream()->time_base);
  }
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID(file_url_);
  dispatcher_->Run();
  NewEvent(kToStart, shared_from_this(), dispatcher_)
    ->Post();
}

void FileMediaSrc::Cancel() {
  canceled_ = true;
  video_ctx_.canceled = true;
  audio_ctx_.canceled = true;
  if (video_filter_) video_filter_->Cancel(&video_ctx_);
  if (audio_filter_) audio_filter_->Cancel(&audio_ctx_);
  NewEvent(kToCancel, shared_from_this(), dispatcher_)
    ->Post();
}

void FileMediaSrc::Close() {
  std::promise<void> pr;
  auto future = pr.get_future();
  NewEvent(kToClose, shared_from_this(), dispatcher_)
    ->SetData("pr", &pr)
    ->Post();
  future.get();
  dispatcher_->Release();
  LOG(VERBOSE) << "file_media_src:" << file_url_ << " closed";
}

void FileMediaSrc::HandleEvent(const sp<Event> &event) {
  switch (event->what()) {
    case kToStart: {
      LOG(VERBOSE) << "file media src to start.";
      PerformStart();
      break;
    }

    case kToLoop: {
      PerformLoop();
      break;
    }

    case kToClose: {
      PerformClose(event);
      break;
    }

    case kToCancel: {
      PerformOnCanceled();
      break;
    }

    case kOnError: {
      PerformOnError();
      break;
    }

    case kOnEnd: {
      PerformOnEnd();
      break;
    }

    default:
      break;
  }
}

void FileMediaSrc::PerformStart() {
  //calibrate range
  using namespace std::chrono;
  auto need_range = std::get<0>(range_);
  auto left_pos_ms = std::get<1>(range_);
  auto right_pos_ms = std::get<2>(range_);

  using namespace std;
  loop_data_.duration_ms = max(demuxer_->video_duration(), demuxer_->audio_duration());
  if (need_range) {
    demuxer_->Seek(-1, MsToTimestamp(left_pos_ms, { 1, AV_TIME_BASE }), AVSEEK_FLAG_BACKWARD);
    loop_data_.duration_ms = min(right_pos_ms, loop_data_.duration_ms) - left_pos_ms;
  } else {
    //you should seek to the beginning of the file, because for the test pkt.
    //demuxer_->Seek(-1, INT_MIN, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
  }
  loop_data_.start_clock = steady_clock::now();
  NewEvent(kToLoop, shared_from_this(), dispatcher_)
    ->Post();
}

void FileMediaSrc::PerformLoop() {
  using namespace std::chrono;
  sp<ARVPacket> pkt;
  auto ret = demuxer_->NextPacket(pkt);

  if (ret == 0) {
    if (pkt->media_type == AVMEDIA_TYPE_VIDEO) {
      if (loop_data_.video_start_dts == INT_MIN && loop_data_.audio_start_dts == INT_MIN) {
        loop_data_.video_start_dts = pkt->dts;
        if (demuxer_->audio_stream_index() >= 0) {
          loop_data_.audio_start_dts = av_rescale_q(loop_data_.video_start_dts,
                                                    demuxer_->video_stream()->time_base,
                                                    demuxer_->audio_stream()->time_base);
        }
        NotifyFirstPktDts();
      }
      pkt->pts -= loop_data_.video_start_dts;
      pkt->dts -= loop_data_.video_start_dts;
    } else if (pkt->media_type == AVMEDIA_TYPE_AUDIO) {
      if (loop_data_.video_start_dts == INT_MIN && loop_data_.audio_start_dts == INT_MIN) {
        loop_data_.audio_start_dts = pkt->dts;
        if (demuxer_->video_stream_index() >= 0) {
          loop_data_.video_start_dts = av_rescale_q(loop_data_.audio_start_dts,
                                                    demuxer_->audio_stream()->time_base,
                                                    demuxer_->video_stream()->time_base);
        }
        NotifyFirstPktDts();
      }
      pkt->pts -= loop_data_.audio_start_dts;
      pkt->dts -= loop_data_.audio_start_dts;
    } else {
      goto LOOP_AGAIN;
    }

    {
      //update progress and check end
      //pts may be changed by filter, so caculate progress first
      auto cur_pkt_dts_ms = TimestampToMs(pkt->dts, demuxer_->StreamAtIndex(pkt->stream_index)->time_base);
      if (realtime_) {
        auto pass_ms = duration_cast<milliseconds>(steady_clock::now() - loop_data_.start_clock);
        if (cur_pkt_dts_ms > pass_ms.count()) {
          auto diff = milliseconds(cur_pkt_dts_ms) - pass_ms;
          std::this_thread::sleep_for(diff);
        }
      }

      progress_ = static_cast<double>(cur_pkt_dts_ms) / loop_data_.duration_ms;
      auto need_range = std::get<0>(range_);
      if (need_range && progress_ >= 1.0) {
        goto LOOP_END;
      }
    }

    {
      //add start time offset
      if (pkt->media_type == AVMEDIA_TYPE_VIDEO) {
        pkt->pts += loop_data_.video_offset_dts_;
        pkt->dts += loop_data_.video_offset_dts_;
        loop_data_.last_video_dts_ = pkt->dts;
      } else if (pkt->media_type == AVMEDIA_TYPE_AUDIO) {
        pkt->pts += loop_data_.audio_offset_dts_;
        pkt->dts += loop_data_.audio_offset_dts_;
        loop_data_.last_audio_dts_ = pkt->dts;
      }
    }
    //        LOG(VERBOSE) << "progress:" << progress_;
    if (pkt->stream_index == demuxer_->video_stream_index()) {
      if (video_filter_ && !video_filter_->Filter(&video_ctx_, pkt)) {
        error_code_ = video_ctx_.error_code;
        goto LOOP_ERROR;
      }
    } else if (pkt->stream_index == demuxer_->audio_stream_index()) {
      if (audio_filter_ && !audio_filter_->Filter(&audio_ctx_, pkt)) {
        error_code_ = audio_ctx_.error_code;
        goto LOOP_ERROR;
      }
    }

  } else if (ret == AVERROR_EOF) {
    goto LOOP_END;
  } else {
    error_code_ = kDemuxError;
    goto LOOP_ERROR;
  }
  
LOOP_AGAIN:
  NewEvent(kToLoop, shared_from_this(), dispatcher_)
    ->Post();
  return;

LOOP_ERROR:
  NewEvent(kOnError, shared_from_this(), dispatcher_)
    ->Post();
  return;

LOOP_END:
  NewEvent(kOnEnd, shared_from_this(), dispatcher_)
    ->Post();
  return;
}

void FileMediaSrc::PerformClose(const sp<Event> &event) {
  LOG(VERBOSE) << "perform close";
  if (NeedLoopVideo()) video_filter_->Close(&video_ctx_);
  if (NeedLoopAudio()) audio_filter_->Close(&audio_ctx_);
  std::promise<void> *pr;
  event->GetDataCopy("pr", pr);
  pr->set_value();
  dispatcher_->Stop();
}

}
