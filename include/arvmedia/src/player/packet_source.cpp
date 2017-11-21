//
//  packet_source.cpp
//
//  Created by jerett on 16/2/16.
//  Copyright © 2016 insta360. All rights reserved.
//

#include "packet_source.h"

#include <future>
#include <map>

#include <av_toolbox/demuxer.h>
#include <eventloop/event.h>
#include <eventloop/event_dispatcher.h>

#include "player_error.h"
#include "url_parser.h"
#include "list_packet_buffer.h"
#include "circular_packet_buffer.h"

namespace ins {

enum {
  kToPrepare,
  kToStart,
  kToSeek,
  kToReadSource,
  kToBuffer,
  kToReadUntilVideo,
  kToClearVideo
};

sp<PacketSource> NewSource(const std::string &url, const sp<Event> &notify) {
  auto source = new PacketSource(url, notify);
  std::shared_ptr<PacketSource> sp_source(source);
  return sp_source;
}

PacketSource::PacketSource(const std::string &url, const sp<Event> &notify)
    :url_(url), notify_(notify) {
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID("PacketSource");
  dispatcher_->Run();
}

PacketSource::~PacketSource() {
  LOG(INFO) << "~PacketSource";
}

void PacketSource::PrepareAsync(const std::unordered_map<std::string, std::string> &options) {
  {
    auto itr = options.find("buffer_ms");
    if (itr != options.end()) {
      LOG(INFO) << "set buffer duration:" << itr->second;
      kBufferDurationInMs = std::stoi(itr->second);
    }
  }

  {
    auto itr = options.find("max_buffer_size");
    if (itr != options.end()) {
      LOG(INFO) << "set max buffer size:" << itr->second;
      kMaxBufferSize = std::stoi(itr->second);
    }
  }

  auto event = NewEvent(kToPrepare, shared_from_this(), dispatcher_);
  event->SetData("options", &options);
  event->Post();
}

int PacketSource::ReadUntilVideoSync(sp<AVPacket> &video_packet) {
  std::promise<int> pr;
  auto future = pr.get_future();
  auto event = NewEvent(kToReadUntilVideo, shared_from_this(), dispatcher_);
  event->SetData("pr", &pr);
  event->Post();
  auto ret = future.get();
  event->GetDataCopy("video_packet", video_packet);
  return ret;
}

void PacketSource::ClearVideoSync() {
  std::promise<void> pr;
  auto future = pr.get_future();
  auto event = NewEvent(kToClearVideo, shared_from_this(), dispatcher_);
  event->SetData("pr", &pr);
  event->Post();
  future.get();
}

void PacketSource::StartAsync() {
  to_release_ = false;
  NewEvent(kToStart, shared_from_this(), dispatcher_)->Post();
}

int PacketSource::SeekSync(int stream_index, int64_t dts, int flags) {
  auto event = NewEvent(kToSeek, shared_from_this(), dispatcher_);
  event->SetData("dts", dts);
  event->SetData("stream_index", stream_index);
  event->SetData("flags", flags);
  std::promise<int> pr;
  auto future = pr.get_future();
  event->SetData("pr", &pr);
  event->Post();
  auto ret = future.get();
  return ret;
}

void PacketSource::BufferSync(const BufferingProgressCallback &progress_callback) {
  auto event = NewEvent(kToBuffer, shared_from_this(), dispatcher_);
  std::promise<bool> pr;
  auto future = pr.get_future();
  event->SetData("pr", &pr);
  event->SetData("progress_callback", progress_callback);
  event->Post();
  future.get();
}

void PacketSource::ReleaseSync() {
  to_release_ = true;
  if (demuxer_) {
    demuxer_->interrupt();
  }

  dispatcher_->Stop();
  dispatcher_->Release();
}

int PacketSource::ReadVideo(sp<AVPacket> &video_packet) {
  auto ret = video_buffer_->PopPacket(video_packet);
  if (ret == kErrorAgain && eof_) {
    return kErrorEnd;
  } else {
    return ret;
  }
}

int PacketSource::ReadAudio(sp<AVPacket> &audio_packet) {
  auto ret = audio_buffer_->PopPacket(audio_packet);
  if (ret == kErrorAgain && eof_) {
    return kErrorEnd;
  } else {
    return ret;
  }
}

void PacketSource::HandleEvent(const sp<ins::Event> &event) {
  switch (event->what()) {
    case kToPrepare:
    {
      Prepare(event);
      break;
    }

    case kToStart:
    {
      NewEvent(kToReadSource, shared_from_this(), dispatcher_)->Post();
      break;
    }

    case kToBuffer:
    {
      BufferingProgressCallback cb;
      event->GetDataCopy("progress_callback", cb);
      Buffer(cb);
      std::promise<bool> *pr;
      event->GetDataCopy("pr", pr);
      pr->set_value(true);
      break;
    }

    case kToReadSource:
    {
      ReadSource();
      break;
    }

    case kToReadUntilVideo:
    {
      std::promise<int> *pr;
      event->GetDataCopy("pr", pr);
      sp<AVPacket> video_packet;
      auto ret = ReadUntilVideo(video_packet);
      event->SetData("video_packet", video_packet);
      pr->set_value(ret);
      break;
    }

    case kToSeek:
    {
      Seek(event);
      break;
    }

    case kToClearVideo:
    {
      std::promise<void> *pr;
      event->GetDataCopy("pr", pr);
      video_buffer_->Clear();
      pr->set_value();
      break;
    }

    default:
    {
      break;
    }
  }
}

int PacketSource::ReadUntilVideo(sp<AVPacket> &video_packet) {
  auto ret = ReadVideo(video_packet);
  if (ret == kErrorNone) {
//    LOG(VERBOSE) << __FUNCTION__ << " from buffer.";
    goto ErrorNone;
  } else if (ret == kErrorAgain) {
    sp<ARVPacket> pkt;
    while ((ret = demuxer_->NextPacket(pkt)) == 0) {
      if (pkt->stream_index == demuxer_->video_stream_index()) {
        video_packet = pkt;
        goto ErrorNone;
      } else if (pkt->stream_index == demuxer_->audio_stream_index()) {
        audio_buffer_->PushPacket(pkt);
      }
    }
    if (ret == AVERROR_EOF) {
      goto ErrorEnd;
    } else {
      goto ErrorDemux;
    }
  } else if (ret == kErrorEnd) {
    goto ErrorEnd;
  }
  CHECK(false) << __FUNCTION__ << " must not run into this path";
  return kErrorNone;

ErrorNone:
  {
    if (demuxer_->audio_stream()) {
      auto audio_dts = av_rescale_q(video_packet->dts,
                                    demuxer_->video_stream()->time_base,
                                    demuxer_->audio_stream()->time_base);
      audio_buffer_->DiscardUntilDTS(audio_dts);
    }
    return kErrorNone;
  }

ErrorEnd:
  {
    eof_ = true;
    return kErrorEnd;
  }

ErrorDemux:
  {
    ins::CloneEventAndAlterType(notify_, kError)
      ->SetData("error", kErrorDemux)
      ->Post();
    return kErrorDemux;
  }
}

void PacketSource::Prepare(const sp<ins::Event> &event) {
  auto prepared_notify = ins::CloneEventAndAlterType(notify_, kPrepared);
  URLParser url_parser;

  {
    //parse URL first
    if (!url_parser.Parse(url_)) {
      prepared_notify->SetData("error", kErrorParseUrl);
      LOG(INFO) << "parse url err";
      return;
    }
    LOG(INFO) << "protocol:" << url_parser.protocol() << " " << "host:" << url_parser.host() << " "
      << "path:" << url_parser.path() << " " << "query:" << url_parser.query();
  }

  const std::unordered_map<std::string, std::string> *options;
  event->GetDataCopy("options", options);

  int ret;
  {
    // open demuxer
    demuxer_.reset(new Demuxer(url_));
    std::map<std::string, std::string> demux_opts;
    {
      auto itr = options->find("rtsp_transport");
      if (url_parser.protocol() == "rtsp" && itr != options->end()) {
        //LOG(INFO) << "set rtsp over tcp";
        LOG(INFO) << "set rtsp over " << itr->second;
        demux_opts.insert({ "rtsp_transport", itr->second});
      }
    }

    {
      auto itr = options->find("zero_latency");
      if (itr != options->end() && itr->second == "true") {
        LOG(INFO) << "zero latency mode.";
        demux_opts.insert({"probesize", "32"});
      }
    }
    ret = demuxer_->Open(&demux_opts);
    LOG(INFO) << "demux " << url_ << " result:" << ret;
  }

  if (ret == 0) {
    prepared_notify->SetData("error", kErrorNone);
    prepared_notify->SetData("duration", demuxer_->duration());
    prepared_notify->SetData("video_stream", demuxer_->video_stream());
    prepared_notify->SetData("video_duration", demuxer_->video_duration());
    prepared_notify->SetData("audio_stream", demuxer_->audio_stream());
    prepared_notify->SetData("audio_duration", demuxer_->audio_duration());

    video_buffer_.reset(new ListPacketBuffer());
    audio_buffer_.reset(new ListPacketBuffer());

    if (url_parser.protocol() == "file") {
    } else {
      if (url_parser.protocol() == "rtsp" || url_parser.protocol() == "rtmp") {
        kBufferDurationInMs = 3 * 1000;
      }
    }
    auto itr = options->find("zero_latency");
    if (itr != options->end() && itr->second == "true") {
      audio_buffer_.reset(new CircularPacketBuffer(15));
    }
    if (demuxer_->video_stream_index() >= 0) {
      wait_for_key_frame_ = true;
    } else {
      wait_for_key_frame_ = false;
    }
  } else {
    prepared_notify->SetData("error", kErrorOpenFormat);
  }

  prepared_notify->Post();
}

void PacketSource::ReadSource() {
  if (BufferIsFull()) {
//    LOG(INFO) << "Packet Source buffer full ";
    NewEvent(kToReadSource, shared_from_this(), dispatcher_)->PostWithDelay(100);
    return;
  }

//  LOG(VERBOSE) << "read pkt:" << video_buffer_size_ + audio_buffer_size_;
  sp<ARVPacket> pkt;
  auto ret = demuxer_->NextPacket(pkt);

  if (ret == 0) {
    if (pkt->media_type == AVMEDIA_TYPE_AUDIO) {
      if (wait_for_key_frame_) goto POST;
      /**
       *  if is live streaming, for zero delay, construct circular buffer.
       */
      audio_buffer_->PushPacket(pkt);
    } else if (pkt->media_type == AVMEDIA_TYPE_VIDEO) {
      if (wait_for_key_frame_ && !IsKeyFrame(pkt)) {
        LOG(INFO) << "wait for key frame type:";
        goto POST;
      }
      if (wait_for_key_frame_ && IsKeyFrame(pkt)) wait_for_key_frame_ = false;
//      LOG(VERBOSE) << "push video packet." << TimestampToMs(pkt->pts, demuxer_->video_stream()->time_base);
      video_buffer_->PushPacket(pkt);
    }
  POST:
    /*
    * when buffer, don't demux again, otherwise player will BufferSync agian, leading to dead lock infinite loop
    */
    NewEvent(kToReadSource, shared_from_this(), dispatcher_)
      ->Post();
  } else {
    if (ret == AVERROR_EOF) {
      LOG(INFO) << "packet source read over ";
      eof_ = true;
    } else {
      LOG(INFO) << "packet source read err";
      ins::CloneEventAndAlterType(notify_, kError)
          ->SetData("error", kErrorDemux)
          ->Post();
    }
  }
}

void PacketSource::Seek(const sp<Event>& event) {
  int64_t dts;
  event->GetDataCopy("dts", dts);

  std::promise<int> *pr;
  event->GetDataCopy("pr", pr);

  int stream_index;
  event->GetDataCopy("stream_index", stream_index);

  int flags;
  event->GetDataCopy("flags", flags);
  {
    video_buffer_->Clear();
    audio_buffer_->Clear();
    auto ret = demuxer_->Seek(stream_index, dts, flags);

    if (ret != 0) {
      LOG(ERROR) << "demux seek err....";
      pr->set_value(kErrorSeek);
    } else {
      if (eof_) {
        NewEvent(kToReadSource, shared_from_this(), dispatcher_)->Post();
        eof_ = false;
      }
      pr->set_value(kErrorNone);
    }
  }
}

void PacketSource::Buffer(const std::function<void(double)> &progress_callback) {
  while (true) {
    if (to_release_) {
      LOG(INFO) << "buffer break to release ....";
      break;
    }

    sp<ARVPacket> pkt;
    auto ret = demuxer_->NextPacket(pkt);

    if (ret == 0) {
      if (pkt->stream_index == demuxer_->audio_stream_index()) {
        if (wait_for_key_frame_) continue;
        audio_buffer_->PushPacket(pkt);
      } else {
        auto is_key = IsKeyFrame(pkt);
        if (wait_for_key_frame_ && !is_key) {
          LOG(INFO) << "wait for key frame type:" ;
          continue;
        }
        if (wait_for_key_frame_ && is_key) wait_for_key_frame_ = false;
        video_buffer_->PushPacket(pkt);
      }
    } else {
      if (ret == AVERROR_EOF) {
        LOG(VERBOSE) << "packet source buffer read over";
        eof_ = true;
      } else {
        LOG(ERROR) << "packet source buffer read err";
        auto err_notify = ins::CloneEventAndAlterType(notify_, kError);
        err_notify->SetData("error", kErrorDemux);
        err_notify->Post();
      }
      break;
    }

    auto buffer_duration = BufferDurationInMs();
    if (buffer_duration >= kBufferDurationInMs || BufferIsFull()) {
      LOG(VERBOSE) << "buffer finish, buffered duration:" << buffer_duration << "ms. "
        << " buffer size:" << (video_buffer_->size() + audio_buffer_->size())/1024 << "KB.";
      progress_callback(1.0);
      break;
    } else if (progress_callback) {
      auto progress = static_cast<double>(buffer_duration) / kBufferDurationInMs;
      progress_callback(progress);
    }
  }
  NewEvent(kToReadSource, shared_from_this(), dispatcher_)->Post();
}

int64_t PacketSource::BufferDurationInMs() const {
  int64_t buffer_duration = -1;
  if (demuxer_->video_stream()) {
    buffer_duration = TimestampToMs(video_buffer_->buffer_duration_in_dts(), demuxer_->video_stream()->time_base);
  }
  if (demuxer_->audio_stream()) {
    auto audio_duration = TimestampToMs(audio_buffer_->buffer_duration_in_dts(), demuxer_->audio_stream()->time_base);
    if (audio_duration > buffer_duration) buffer_duration = audio_duration;
  }
  return buffer_duration;
}

}
