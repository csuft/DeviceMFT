//
//  audio_decoder.hpp
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/22.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef audio_decoder_h
#define audio_decoder_h

#include <stdint.h>
#include <string.h>
extern "C" {
#include <libavformat/avformat.h>
}
#include "eventloop/event_handler.h"
#include "av_toolbox/sp.h"

struct AVStream;
struct AVPacket;

namespace ins {
class Event;
class AudioDecodeHandler;
class Decoder;

class AudioDecodeHandler : public EventHandler, public std::enable_shared_from_this<AudioDecodeHandler>{
  friend sp<AudioDecodeHandler> CreateAudioDecoder(const AVStream *audio_stream, const sp<Event> &notify);
public:
  enum {
    kNeedInputAudioPacket = 4000,
    kDecodedAudioPacket,
  };
  AudioDecodeHandler(const AudioDecodeHandler&) = delete;
  AudioDecodeHandler& operator=(const AudioDecodeHandler&) = delete;
  ~AudioDecodeHandler();
  bool StartAsync();
  void ReleaseSync();
  bool PrepareSync();
  void DecodeAudioAsync(const sp<AVPacket> &audio_packet);
  bool CheckEvent(const sp<Event> &event) const;

private:
  AudioDecodeHandler(up<Decoder> &&decoder, const AVStream *stream, const sp<Event> &notify) noexcept;
  void HandleEvent(const sp<ins::Event> &event) override;
  void DecodeAudio(const sp<Event> &event);
  void UpdateSerialNum();

private:
  AVRational stream_time_base_;
  up<Decoder> decoder_;
  sp<Event> notify_;
  sp<EventDispatcher> dispatcher_;
  int64_t serial_num_ = 0;
};

sp<AudioDecodeHandler> CreateAudioDecoder(const AVStream *audio_stream, const sp<Event> &notify);

}

#endif /* audio_decoder_h */
