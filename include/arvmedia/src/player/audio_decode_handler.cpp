//
//  audio_decoder.cpp
//
//  Created by jerett on 16/2/22.
//  Copyright © 2016 insta360. All rights reserved.
//

#include "audio_decode_handler.h"
#include <llog/llog.h>
#include "eventloop/event.h"
#include "eventloop/event_dispatcher.h"
#include "av_toolbox/ffmpeg_util.h"
#include "av_toolbox/decoder.h"

namespace ins {
enum {
  kToStart,
  kToDecodeAsync,
};

sp<AudioDecodeHandler> CreateAudioDecoder(const AVStream *stream,
                                          const sp<Event> &notify) {
//  #if (__APPLE__)
//  sp<AVCodecContext> dec_ctx;
//  if (stream->codecpar->codec_id == AV_CODEC_ID_AAC) {
//    dec_ctx = NewAVDecodeContextFromName("aac_at");
//    LOG(VERBOSE) << "using AudioToolbox decode.";
//  } else {
//    dec_ctx = NewAVDecodeContextFromID(stream->codecpar->codec_id);
//  }
//  #else
//  auto dec_ctx = NewAVDecodeContextFromID(stream->codecpar->codec_id);
//  #endif
  auto dec_ctx = NewAVDecodeContextFromID(stream->codecpar->codec_id);
  dec_ctx->refcounted_frames = 1;
  CHECK(avcodec_parameters_to_context(dec_ctx.get(), stream->codecpar) == 0);
  up<Decoder> decoder(new Decoder(dec_ctx));
  sp<AudioDecodeHandler> audio_decoder(new AudioDecodeHandler(std::move(decoder), stream, notify));
  return audio_decoder;
}

AudioDecodeHandler::AudioDecodeHandler(up<Decoder> &&decoder, const AVStream *stream, const sp<Event> &notify) noexcept
    : stream_time_base_(stream->time_base), decoder_(std::move(decoder)), notify_(notify) {
  notify_->SetData("audio_decoder", this);
  notify_->SetData("serial_num", serial_num_);
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID("AudioDecoder");
}

AudioDecodeHandler::~AudioDecodeHandler() {
}

bool AudioDecodeHandler::PrepareSync() {
  return decoder_->Open() == 0;
}

void AudioDecodeHandler::ReleaseSync() {
  dispatcher_->Stop();
  dispatcher_->Release();
}

bool AudioDecodeHandler::StartAsync() {
  dispatcher_->Run();
  ins::CloneEventAndAlterType(notify_, kNeedInputAudioPacket)->Post();
  return true;
}

void AudioDecodeHandler::DecodeAudioAsync(const sp<AVPacket> &audio_packet) {
  if (audio_packet) {
    auto event = NewEvent(kToDecodeAsync, shared_from_this(), dispatcher_);
    event->SetData("packet", audio_packet);
    event->Post();
  }
}

bool AudioDecodeHandler::CheckEvent(const sp<Event>& event) const {
  int64_t serial_num;
  AudioDecodeHandler *handler = nullptr;
  if (!event->GetDataCopy("serial_num", serial_num)) return false;
  if (!event->GetDataCopy("audio_decoder", handler)) return false;
  return handler == this && serial_num <= serial_num_;
}

void AudioDecodeHandler::HandleEvent(const sp<ins::Event> &event) {
  auto what = event->what();
  switch(what) {
    case kToDecodeAsync:{
      DecodeAudio(event);
      break;
    }
    default:
      break;
  }
}

void AudioDecodeHandler::DecodeAudio(const sp<Event> &event) {
  UpdateSerialNum();

  sp<AVPacket> audio_packet;
  event->GetDataCopy("packet", audio_packet);
  auto ret = decoder_->SendPacket(audio_packet);
  sp<AVFrame> frame;
  ret = decoder_->ReceiveFrame(frame);
  if (ret == 0) {
    frame->pts = TimestampToMs(frame->pts, stream_time_base_);
    frame->pkt_dts = TimestampToMs(frame->pkt_dts, stream_time_base_);

    ins::CloneEventAndAlterType(notify_, kDecodedAudioPacket)
        ->SetData("pcm", frame)
        ->Post();
  } else if (ret == AVERROR(EAGAIN)) {
    ;
  } else if (ret == AVERROR_EOF) {
    ;
  } else {
//    LOG(ERROR) << "decode auto err:" << FFmpeg
  }

//  NeAACDecFrameInfo aacDecFrameInfo;
//  char* pcm = (char*)faacDecDecode(handle_,
//                                   &aacDecFrameInfo,
//                                   audio_packet->data,
//                                   audio_packet->len);
//  if (pcm == 0 || aacDecFrameInfo.error > 0 || aacDecFrameInfo.samples <= 0) {
//    LOG(INFO) << "audio decode err:" << aacDecFrameInfo.error << " samples:" << aacDecFrameInfo.samples;
//  } else {
//    int64_t timestamp = audio_packet->pts*1000/audio_info_->audio_timescale;
//    auto pcm_data = std::make_shared<MediaData>(pcm, 4096, timestamp);
//
//    if (aacDecFrameInfo.channels > 2) {
//      for (int i = 0; i < 1024; ++i) {
//        for (int j = 0; j < 4; ++j) {
//          (*pcm_data)[4*i + j] = static_cast<uint8_t>(pcm[12*i + j]);
//        }
//      }
//    }
//  }
  //keep feed
  ins::CloneEventAndAlterType(notify_, kNeedInputAudioPacket)->Post();
}

void AudioDecodeHandler::UpdateSerialNum() {
  ++serial_num_;
  notify_->SetData("serial_num", serial_num_);
}

}

