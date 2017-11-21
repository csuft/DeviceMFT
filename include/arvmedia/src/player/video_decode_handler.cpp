//
//  video_decoder.cpp
//
//  Created by jerett on 16/2/18.
//  Copyright © 2016 insta360. All rights reserved.
//

#include "video_decode_handler.h"

#include <future>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <av_toolbox/ffmpeg_util.h>
#include <av_toolbox/decoder.h>
#include <eventloop/event.h>
#include <eventloop/event_dispatcher.h>

namespace ins {

enum {
  kToDecodeAsync,
  kToDecodeSync,
  kToSendEOF,
  kToFlushImage,
  kToFlushBuffer,
};

sp<VideoDecodeHandler> CreateVideoDecodeHandler(const std::unordered_map<std::string, std::string> &options,
                                                const AVStream *stream,
                                                const sp<Event> &notify) {
  bool enable_hwaccel = true;
  auto itr = options.find("enable_hwaccel");
  if (itr != options.end() && itr->second == "false") {
    enable_hwaccel = false;
    LOG(INFO) << "use cpu decode";
  }
  sp<AVCodecContext> dec_ctx = nullptr;
  if (enable_hwaccel) {
    dec_ctx = NewAVHwaccelDecodeContextFromID(stream->codecpar->codec_id, true);
  }
  if (dec_ctx == nullptr) {
    dec_ctx = NewAVDecodeContextFromID(stream->codecpar->codec_id);
  }
  LOG(VERBOSE) << "to use dec:" << dec_ctx->codec->name;
  dec_ctx->refcounted_frames = 1;
  CHECK(avcodec_parameters_to_context(dec_ctx.get(), stream->codecpar) == 0);
  up<Decoder> decoder(new Decoder(dec_ctx));
  sp<VideoDecodeHandler> video_decoder(new VideoDecodeHandler(std::move(decoder), stream->time_base, notify));
  return video_decoder;
}

VideoDecodeHandler::VideoDecodeHandler(up<Decoder> &&decoder, const AVRational &r, const sp<Event> &notify) noexcept
    :stream_time_base_(r), decoder_(std::move(decoder)), notify_(notify) {
  notify_->SetData("video_decoder", this);
  notify_->SetData("serial_num", serial_num_);
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID("VideoDecoder");
}

VideoDecodeHandler::~VideoDecodeHandler() {
  LOG(INFO) << "~VideoDecoder";
}

bool VideoDecodeHandler::PrepareSync() {
  return decoder_->Open() == 0;
}

bool VideoDecodeHandler::StartAsync() {
  dispatcher_->Run();
  CloneEventAndAlterType(notify_, kNeedInputVideoPacket)->Post();
  return true;
}

void VideoDecodeHandler::ReleaseSync() {
  dispatcher_->Stop();
  dispatcher_->Release();
}

bool VideoDecodeHandler::CheckEvent(const sp<Event>& event) const {
  int64_t serial_num;
  VideoDecodeHandler *handler = nullptr;
  if (!event->GetDataCopy("serial_num", serial_num)) return false;
  if (!event->GetDataCopy("video_decoder", handler)) return false;
//  if (serial_num > serial_num_) {
//    LOG(VERBOSE) << "to discard video ......." << serial_num << " " << serial_num_;
//  }
//  LOG(VERBOSE) << "check video serial:" << serial_num << " " << serial_num_;

  return handler == this && serial_num <= serial_num_;
}

void VideoDecodeHandler::DecodeVideoAsync(const sp<AVPacket> &video_packet) {
  if (video_packet) {
    auto event = NewEvent(kToDecodeAsync, shared_from_this(), dispatcher_);
    event->SetData("packet", video_packet);
    event->PostWithDelay(0);
  }
}

sp<AVFrame> VideoDecodeHandler::DecodeVideoSync(const sp<AVPacket> &video_packet) {
  if (video_packet) {
    std::promise<sp<AVFrame>> pr;
    auto future = pr.get_future();
    NewEvent(kToDecodeSync, shared_from_this(), dispatcher_)
      ->SetData("packet", video_packet)
      ->SetData("pr", &pr)
      ->Post();
    return future.get();
  }
  return nullptr;
}

void VideoDecodeHandler::FlushBufferSync() {
  std::promise<void> pr;
  auto future = pr.get_future();

  NewEvent(kToFlushBuffer, shared_from_this(), dispatcher_)
    ->SetData("pr", &pr)
    ->Post();
  return future.get();
}

void VideoDecodeHandler::SendEOF() {
  std::promise<void> pr;
  auto future = pr.get_future();

  NewEvent(kToSendEOF, shared_from_this(), dispatcher_)
      ->SetData("pr", &pr)
      ->Post();
  return future.get();
}

sp<AVFrame> VideoDecodeHandler::FlushImageSync() {
  std::promise<sp<AVFrame>> pr;
  auto future = pr.get_future();

  NewEvent(kToFlushImage, shared_from_this(), dispatcher_)
      ->SetData("pr", &pr)
      ->PostWithDelay(0);
  return future.get();
}

sp<AVFrame> VideoDecodeHandler::DecodePacket(const sp<AVPacket> &video_packet) {
  auto ret = decoder_->SendPacket(video_packet);
  sp<AVFrame> frame;
  ret = decoder_->ReceiveFrame(frame);
  if (ret == 0 && frame->pts != AV_NOPTS_VALUE) {
    frame->pts = TimestampToMs(frame->pts, stream_time_base_);
    frame->pkt_dts = TimestampToMs(frame->pkt_dts, stream_time_base_);
//    LOG(VERBOSE) << "decode pkt:" << frame->pts;
  }
  return frame;
}

void VideoDecodeHandler::UpdateSerialNum() {
  ++serial_num_;
  notify_->SetData("serial_num", serial_num_);
}

void VideoDecodeHandler::HandleEvent(const sp<Event> &event) {
  auto what = event->what();
  switch(what) {
    case kToDecodeAsync:
    {
      sp<AVPacket> video_packet;
      event->GetDataCopy("packet", video_packet);
      auto frame = DecodePacket(video_packet);

      UpdateSerialNum();
      if (frame) {
        CloneEventAndAlterType(notify_, kDecodedVideoPacket)
          ->SetData("img", frame)
          ->Post();
      }
      CloneEventAndAlterType(notify_, kNeedInputVideoPacket)->Post();
      break;
    }

    case kToDecodeSync:
    {
      sp<AVPacket> video_packet;
      event->GetDataCopy("packet", video_packet);
      std::promise<sp<AVFrame>> *pr;
      event->GetDataCopy("pr", pr);
      auto frame = DecodePacket(video_packet);
      pr->set_value(frame);
      break;
    }

    case kToSendEOF:
    {
      std::promise<void> *pr;
      event->GetDataCopy("pr", pr);
      decoder_->SendEOF();
      pr->set_value();
      break;
    }

    case kToFlushImage:
    {
      std::promise<sp<AVFrame>> *pr;
      event->GetDataCopy("pr", pr);
      sp<AVFrame> frame;
      auto ret = decoder_->ReceiveFrame(frame);
      if (ret == 0 && frame->pts != AV_NOPTS_VALUE) {
        frame->pts = TimestampToMs(frame->pts, stream_time_base_);
        frame->pkt_dts = TimestampToMs(frame->pkt_dts, stream_time_base_);
      }
      pr->set_value(frame);
      break;
    }

    case kToFlushBuffer:
    {
      std::promise<void> *pr;
      event->GetDataCopy("pr", pr);
      serial_num_ = 0;
      decoder_->FlushBuffer();
      pr->set_value();
      break;
    }

    default:
    {
      break;
    }
  }
}

}
