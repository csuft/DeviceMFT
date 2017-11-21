//  player.cpp
//
//  Created by jerett on 16/2/17.
//  Copyright © 2016 insta360. All rights reserved.
//

#include "player.h"
#if _WIN32
#define NOMINMAX
#endif // _WIN32

#include <future>
#include <algorithm>
#include <llog/llog.h>
#include <eventloop/event.h>
#include <eventloop/event_dispatcher.h>

#include "video_decode_handler.h"
#include "audio_decode_handler.h"
#include "renderer.h"
#include "player_error.h"
#include "packet_source.h"

namespace ins {
enum PlayerInternalStatus {
  kStatusPreparing = 100,
  kStatusReleased,
};

enum {
  kToPrepare,
  kToPlay,
  KToPause,
  kToSeek,
  kToRelease,
};

Player::Player(const std::string &url): url_(url) {
  InitFFmpeg();
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID("Player");
  dispatcher_->Run();
}

Player::~Player() noexcept {
  CHECK(status_ == kStatusReleased) << "You Must Call ReleaseSync before ~";
  LOG(VERBOSE) << "~Player";
}

void Player::SetVideoRenderer(const RenderFunc& render_func) {
  render_func_ = render_func;
  if (renderer_) {
    renderer_->SetVideoRenderer(render_func);
  }
}

sp<AudioTransform> Player::set_audio_transform(const sp<AudioTransform> &transform) {
  audio_transform_ = transform;
  if (renderer_) {
    renderer_->set_audio_transform(transform);
  }
  return transform;
}

void Player::PrepareAsync() {
  CHECK(status_ == kStatusUnKnown) << "PrepareAsync when status not Unknows:" << status_;
  status_ = kStatusPreparing;
  NewEvent(kToPrepare, shared_from_this(), dispatcher_)->Post();
}

void Player::PlayAsync() {
  NewEvent(kToPlay, shared_from_this(), dispatcher_)->Post();
}

void Player::PauseAsync() {
  if (media_type_ == kRealtimeStreamMedia) return;
  LOG(VERBOSE) << "to Pause.";
  NewEvent(KToPause, shared_from_this(), dispatcher_)->Post();
}

void Player::SeekAsync(int64_t position_ms, bool seek_to_key) {
  if (media_type_ == kRealtimeStreamMedia) return;

  last_seek_operation_.seek_position = position_ms;
  last_seek_operation_.seek_to_key = seek_to_key;

  if (seek_status_ == kStatusToSeek) {
    if (status_ == kStatusPaused) {
      interrupt_paused_seek_ = true;// we need to handle next request as soon as possible
    }
    //LOG(INFO) << "discard seek:" << position_ms;
  } else {
    auto cmd = NewEvent(kToSeek, shared_from_this(), dispatcher_);
    cmd->SetData("position", position_ms);
    cmd->SetData("seek_to_key", seek_to_key);
    cmd->Post();
    seek_status_ = kStatusToSeek;
  }
}

void Player::ReleaseSync() {
  if (status_ == kStatusReleased) {
    return;
  }

  LOG(INFO) << "###########ReleaseSync";
  if (packet_source_) packet_source_->BreakBuffer();
  std::promise<bool> pr;
  auto future = pr.get_future();

  auto event = NewEvent(kToRelease, shared_from_this(), dispatcher_);
  event->SetData("pr", &pr);
  event->Post();

  auto result = future.get();
  LOG(VERBOSE) << "Player Release:" << result;
  dispatcher_->Release();
 
  options_.clear();
  notifier_ = nullptr;
  video_stream_ = nullptr;
  audio_stream_ = nullptr;

  status_ = kStatusReleased;
  LOG(INFO) << "###########################Released.";
}

int64_t Player::CurrentPosition() const {
  if (media_type_ == kRealtimeStreamMedia || !renderer_) return -1;
  if (seek_status_ != kStatusToSeek) {
    return renderer_->CurrentPosition();
  } else {
    return last_seek_operation_.seek_position;
  }
}

int Player::GetVideoWidth() const {
  if (!video_stream_) return -1;
  return video_stream_->codecpar->width;
}

int Player::GetVideoHeight() const {
  if (!video_stream_) return -1;
  return video_stream_->codecpar->height;
}

int64_t Player::GetDuration() const {
  if (media_type_ == kRealtimeStreamMedia) return -1;
//  return std::max<int64_t>(video_duration, audio_duration);
  return duration_;
}

double Player::GetFramerate() const {
  if (video_stream_) return static_cast<double>(video_stream_->avg_frame_rate.num) / video_stream_->avg_frame_rate.den;
  return -1;
}

void Player::HandleEvent(const sp<ins::Event> &notify) {
  //TimedBlock(obj, "handle event.");
  auto what = notify->what();
  switch (what) {
    case PacketSource::kPrepared:
    {
      HandleSourcePrepared(notify);
      break;
    }

    case PacketSource::kError:
    {
      status_ = kStatusFailed;
      NotifyStatus();
      break;
    }

    case VideoDecodeHandler::kNeedInputVideoPacket:
    {
      HandleFeedVideoPacket(notify);
      break;
    }

    case VideoDecodeHandler::kDecodedVideoPacket:
    {
      HandleGetDecodedVideo(notify);
      break;
    }

    case AudioDecodeHandler::kNeedInputAudioPacket:
    {
      HandleFeedAudioPacket(notify);
      break;
    }

    case AudioDecodeHandler::kDecodedAudioPacket:
    {
      HandleGetDecodedAudio(notify);
      break;
    }

    case Renderer::kRenderEof:
    {
      if (!renderer_->CheckEvent(notify)) {
        LOG(VERBOSE) << "discard render eof event.";
      } else {
        OnEnd();
      }
      break;
    }

    case kToPrepare:
    {
      Prepare();
      break;
    }

    case kToPlay:
    {
      if (status_ == kStatusPreparing) {
        NewEvent(kToPlay, shared_from_this(), dispatcher_)->PostWithDelay(100);
      } else if (status_ == kStatusPrepared) {
        Play();
        LOG(INFO) << "Player Play";
      } else if (status_ == kStatusPaused) {
        Resume();
        LOG(INFO) << "Player Resume";
      } else if (status_ == kStatusEnd) {
        Replay();
        LOG(INFO) << "Player Replay";
      }
      break;
    }

    case KToPause:
    {
      Pause();
      break;
    }

    case kToSeek:
    {
      int64_t position;
      bool seek_to_key;
      notify->GetDataCopy("position", position);
      notify->GetDataCopy("seek_to_key", seek_to_key);
      Seek(position, seek_to_key);
      break;
    }

    case kToRelease:
    {
      if (packet_source_) packet_source_->ReleaseSync();
      if (video_handler_) video_handler_->ReleaseSync();
      if (audio_handler_) audio_handler_->ReleaseSync();
      if (renderer_) renderer_->ReleaseSync();
      std::promise<bool> *pr;
      notify->GetDataCopy<std::promise<bool>*>("pr", pr);
      pr->set_value(true);
      dispatcher_->Stop();
      break;
    }

    default:
    {
      CHECK(1 == 0) << "unhandle Player msg:" << what;
      break;
    }
  }
}

void Player::CheckBuffer() {
  if (media_type_ == kFileMedia && status_ == kStatusPlaying && buffer_flags.all()) {
    LOG(INFO) << "################to buffer ...";
    renderer_->PauseSync();
    status_ = kStatusBuffering;
    NotifyStatus();
    packet_source_->BufferSync([=](double progress) {
  //    LOG(VERBOSE) << "buffer progress:" << progress;
      NotifyBufferStatus(static_cast<int>(progress * 100));
    });
    renderer_->ResumeSync();
    status_ = kStatusPlaying;
    NotifyStatus();
    LOG(INFO) << "##############buffer finished...";
  }
}

void Player::HandleGetDecodedAudio(const sp<Event> &notify) {
  if (!audio_handler_->CheckEvent(notify)) {
    LOG(VERBOSE) << "discard invalid pcm";
  } else {
    sp<AVFrame> pcm;
    notify->GetDataCopy("pcm", pcm);
    renderer_->EnqueueAudio(pcm);
  }
}

void Player::HandleGetDecodedVideo(const sp<Event> &notify) {
  if (!video_handler_->CheckEvent(notify)) {
    LOG(VERBOSE) << "discard invalid img";
  } else {
    sp<AVFrame> img;
    notify->GetDataCopy("img", img);
    renderer_->EnqueueVideo(img);
  }
}

void Player::HandleSourcePrepared(const sp<Event> &notify) {
  int err;
  notify->GetDataCopy("error", err);
  if (err == kErrorNone) {
    LOG(VERBOSE) << "player Prepare OK";
    status_ = kStatusPrepared;

    //parse video
    notify->GetDataCopy("duration", duration_);
    notify->GetDataCopy("video_stream", video_stream_);
    notify->GetDataCopy("video_duration", video_duration_);
    notify->GetDataCopy("audio_stream", audio_stream_);
    notify->GetDataCopy("audio_duration", audio_duration_);

    {
      if (!video_stream_ && !audio_stream_) {
        status_ = kStatusFailed;
        NotifyStatus();
        return;
      }
    }

    {
      auto itr = options_.find("zero_latency");
      if (itr != options_.end() && itr->second == "true") {
        media_type_ = kRealtimeStreamMedia;
      }
      LOG(VERBOSE) << "media type:" << media_type_;
    }

    //prepare video and audio
    {
      LOG(INFO) << "file duration:" << duration_;
      if (video_stream_) {
        auto video_notify = NewEvent(0, shared_from_this(), dispatcher_);
        video_handler_ = CreateVideoDecodeHandler(options_ ,video_stream_, video_notify);
        auto ret = video_handler_->PrepareSync();
        if (!ret) {
          LOG(ERROR) << "video stream prepared failed";
          status_ = kStatusFailed;
          NotifyStatus();
          return;
        }
        LOG(INFO) << "found video stream. width:" << GetVideoWidth()
          << " height:" << GetVideoHeight()
          << " duration:" << video_duration_
          << " framerate:" << GetFramerate();
      } else {
        LOG(ERROR) << "video stream not found.";
      }

      if (audio_stream_) {
        auto audio_notify = NewEvent(0, shared_from_this(), dispatcher_);
        audio_handler_ = CreateAudioDecoder(audio_stream_, audio_notify);
        auto ret = audio_handler_->PrepareSync();

        if (!ret) {
          LOG(ERROR) << "audio stream prepared failed";
          status_ = kStatusFailed;
          NotifyStatus();
          return;
        }
        LOG(INFO) << "found audio stream."
          << " duration:" << audio_duration_;
      } else {
        LOG(INFO) << "no audio stream.";
      }

      {
        auto render_notify = NewEvent(0, shared_from_this(), dispatcher_);
        renderer_ = CreateRenderer(video_stream_, audio_stream_, media_type_, render_notify);
        if (render_func_) renderer_->SetVideoRenderer(render_func_);
        if (audio_transform_) renderer_->set_audio_transform(audio_transform_);
        auto ret = renderer_->Prepare();
        if (!ret) {
          LOG(ERROR) << "renderer prepared failed";
          status_ = kStatusFailed;
          NotifyStatus();
          return;
        }
      }
    }

    status_ = kStatusPrepared;
    NotifyStatus();
  } else {
    LOG(ERROR) << "player Prepare err:" << err;
    status_ = kStatusFailed;
    NotifyStatus();
  }
}

void Player::Prepare() {
  auto notify = NewEvent(0, shared_from_this(), dispatcher_);
  packet_source_ = NewSource(url_, notify);
  packet_source_->PrepareAsync(options_);
}

void Player::Play() {
  CHECK(status_ == kStatusPrepared) << "Play when status not Prepared " << status_;
  packet_source_->StartAsync();
  if (video_handler_) video_handler_->StartAsync();
  if (audio_handler_) audio_handler_->StartAsync();
  if (renderer_) renderer_->StartAsync();
  video_source_end_ = false;
  audio_source_end_ = false;
  status_ = kStatusPlaying;
  NotifyStatus();
}

void Player::Pause() {
  CHECK(status_ == kStatusBuffering ||
        status_ == kStatusPlaying)
    << "must Pause on Playing or Buffering";
  renderer_->PauseSync();
  status_ = kStatusPaused;
  NotifyStatus();
  LOG(INFO) << "player paused.";
}

void Player::Resume() {
  CHECK(status_ == kStatusPaused)
    << "Resume when status not Resumed " << status_;
  renderer_->ResumeSync();
  status_ = kStatusPlaying;
  NotifyStatus();
}

void Player::Seek(int64_t position_ms, bool seek_to_key) {
  CHECK(status_ == kStatusPaused ||
      status_ == kStatusPlaying ||
      status_ == kStatusEnd) << "must Seek on Paused or Playing or End";
  LOG(INFO) << "################" << "To seek:" << position_ms << " seek_to_key:" << seek_to_key;

  {
    if (video_stream_) video_handler_->FlushBufferSync();
  }

  {
    if (video_stream_ && audio_duration_ == -1) {    //some format don't have video and audio duration, so if have video stream, seek to video only.
      SeekVideo(position_ms, seek_to_key);
    } else if (!video_stream_) {  //if audio only, seek audio
      SeekAudio(position_ms, seek_to_key);
    } else {
      if (position_ms <= video_duration_) { 
        SeekVideo(position_ms, seek_to_key);
      } else {
        SeekAudio(position_ms, seek_to_key);
      }
    }
  }

  {
    video_source_end_ = false;
    audio_source_end_ = false;

    if (status_ == kStatusEnd) {
      status_ = kStatusPaused;
      NotifyStatus();
    }
    seek_status_ = kStatusIdle;
    interrupt_paused_seek_ = false;

    if (position_ms != last_seek_operation_.seek_position) {
      SeekAsync(last_seek_operation_.seek_position, last_seek_operation_.seek_to_key);
      LOG(INFO) << "post discard last seek request:" << position_ms;
    }
    LOG(INFO) << "#######################Player seek->" << position_ms << " clock:" << renderer_->CurrentPosition();
  }
}

void Player::SeekVideo(int64_t position_ms, bool seek_to_key) {
  LOG(VERBOSE) << " Seek video";
  auto seek_dts = MsToTimestamp(position_ms, video_stream_->time_base);
  auto ret = packet_source_->SeekSync(video_stream_->index, seek_dts, AVSEEK_FLAG_BACKWARD);
//  auto seek_dts = MsToTimestamp(audio_stream_->index, audio_stream_->time_base);
//  auto ret = packet_source_->SeekSync(audio_stream_->index, seek_dts, AVSEEK_FLAG_BACKWARD);

  if (ret == kErrorSeek) {
    LOG(ERROR) << "################## Seek to " << position_ms << " Error.";
    return;
  }

  sp<AVFrame> video_img;
  if (seek_to_key) {
    //only decode key frame
    while (video_img == nullptr) {
      sp<AVPacket> video_packet;
      auto ret = packet_source_->ReadUntilVideoSync(video_packet);
      if (ret == kErrorNone) {
        video_img = video_handler_->DecodeVideoSync(video_packet);
      } else if (ret == kErrorEnd) {
        video_handler_->SendEOF();
        video_img = video_handler_->FlushImageSync();
        break;
      } else if (ret == kErrorDemux) {
        LOG(ERROR) << "seek to " << position_ms << " kErrorDemux," << " error will post from packet source";
        return;
      }
    }
  } else {
    //decode sequence to the nearest pts frame
    while (true) {
      sp<AVPacket> video_packet;
      auto ret = packet_source_->ReadUntilVideoSync(video_packet);
      if (ret == kErrorNone) {
        video_img = video_handler_->DecodeVideoSync(video_packet); //video_img may be nullptr due to B frame
        if (video_img == nullptr) continue;
        LOG(VERBOSE) << "seek decoded " << video_img->pts;
        if (video_img->pts >= position_ms || interrupt_paused_seek_) break;
      } else if (ret == kErrorEnd) {
        video_handler_->SendEOF();
        sp<AVFrame> flush_img;
        while ((flush_img = video_handler_->FlushImageSync()) != nullptr) {
          video_img = flush_img;
          if (video_img->pts >= position_ms || interrupt_paused_seek_) {
            break;
          }
        }
        break;
      } else if (ret == kErrorDemux) {
        LOG(ERROR) << "seek to " << position_ms << " kErrorDemux," << " error will post from packet source";
        return;
      }
    }
  }

  if (video_img != nullptr) {
    renderer_->RenderVideoSync(video_img);
    renderer_->SeekSync(video_img->pts);
  }
//  CHECK(video_img != nullptr);
  //LOG(INFO) << "#######################Player seek->" << position_ms << " clock:" << media_clock_->NowInMs();
}

void Player::SeekAudio(int64_t position_ms, bool seek_to_key) {
  LOG(VERBOSE) << " Seek audio";
  auto seek_dts = MsToTimestamp(position_ms, audio_stream_->time_base);
  auto ret = packet_source_->SeekSync(audio_stream_->index, seek_dts, 0);
  if (ret == kErrorSeek) {
    LOG(ERROR) << "################## Seek to " << position_ms << " Error.";
    return;
  }
  renderer_->SeekSync(position_ms);
}

void Player::Replay() {
  CHECK(status_ == kStatusEnd)
    << "Replay when status not End " << status_;
  LOG(VERBOSE) << "to replay:" << video_stream_->first_dts;
  SeekAsync(TimestampToMs(video_stream_->first_dts, video_stream_->time_base), true);
  PlayAsync();
}

void Player::OnEnd() {
  //may seek to end, so add status check.
  if (status_ == kStatusPlaying) {
    renderer_->PauseSync();
  }
  status_ = kStatusEnd;
  NotifyStatus();
  LOG(INFO) << "####################On END##############";
}

void Player::HandleFeedVideoPacket(const sp<Event> &notify) {
  if (renderer_->VideoBufferIsFull()) {
    notify->PostWithDelay(200);
  } else {
    sp<AVPacket> video_packet;
    auto err = packet_source_->ReadVideo(video_packet);
    //LOG(VERBOSE) << "handle feed video pkt:" << err;
    buffer_flags.reset(kBufferFlagVideoIndex);
    if (err == kErrorNone) {
//      LOG(VERBOSE) << "feed video packet." << TimestampToMs(video_packet->pts, video_stream_->time_base);
      video_handler_->DecodeVideoAsync(video_packet);
    } else {
      notify->PostWithDelay(50);
      if (err == kErrorAgain) {
        buffer_flags.set(kBufferFlagVideoIndex, true);
        CheckBuffer();
        buffer_flags.reset();
      } else if (err == kErrorEnd && !video_source_end_) {
        LOG(VERBOSE) << "read video pkt source end";
        // flush
        video_handler_->SendEOF();
        while (status_ != kStatusEnd) {
          auto video_img = video_handler_->FlushImageSync();
          if (!video_img) break;
          renderer_->EnqueueVideo(video_img);
          LOG(VERBOSE) << "flush img:" << video_img->pts;
        }
        renderer_->EnqueueVideoEOF();
        video_source_end_ = true;
        LOG(VERBOSE) << "video source end ";
      }
    }
  }
}

void Player::HandleFeedAudioPacket(const sp<Event> &notify) {
  if (renderer_->AudioBufferIsFull()) {
    notify->PostWithDelay(100);
  } else {
    sp<AVPacket> audio_packet;
    auto err = packet_source_->ReadAudio(audio_packet);
    buffer_flags.reset(kBufferFlagAudioIndex);
    if (err == kErrorNone) {
      audio_handler_->DecodeAudioAsync(audio_packet);
    } else {
      notify->PostWithDelay(50);
      if (err == kErrorAgain) {
        buffer_flags.set(kBufferFlagAudioIndex);
        CheckBuffer();
        buffer_flags.reset();
      } else if (err == kErrorEnd && !audio_source_end_) {
        renderer_->EnqueueAudioEOF();
        audio_source_end_ = true;
        LOG(INFO) << "audio source end ";
      }
    }
  }
}

void Player::NotifyStatus() const  {
  if (notifier_) notifier_(kNotifyStatus, status_, 0);
}

void Player::NotifyBufferStatus(int progress) const {
  if (notifier_) notifier_(kNotifyBufferingProgress, progress, 0);
}

}
