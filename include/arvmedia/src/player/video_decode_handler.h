//
//  video_decoder.h
//
//  Created by jerett on 16/2/18.
//  Copyright © 2016 insta360. All rights reserved.
//

#pragma once

#include <memory>
#include <unordered_map>
extern "C" {
#include <libavutil/rational.h>
}
#include <av_toolbox/sp.h>
#include <eventloop/event_handler.h>

struct AVStream;
struct AVFrame;
struct AVPacket;

namespace ins {
class Event;
class EventDispatcher;
class Decoder;

class VideoDecodeHandler : public EventHandler, public std::enable_shared_from_this<VideoDecodeHandler> {
  friend sp<VideoDecodeHandler> CreateVideoDecodeHandler(const std::unordered_map<std::string, std::string> &options,
                                                         const AVStream *video_stream,
                                                         const sp<Event> &notify);

public:
  enum {
    kNeedInputVideoPacket = 3000,
    kDecodedVideoPacket,
  };
  ~VideoDecodeHandler();

  bool PrepareSync();
  bool StartAsync();
  void DecodeVideoAsync(const sp<AVPacket> &video_packet);
  sp<AVFrame> DecodeVideoSync(const sp<AVPacket> &video_packet);
  void SendEOF();
  sp<AVFrame> FlushImageSync();
  void FlushBufferSync();
  void ReleaseSync();
  bool CheckEvent(const sp<Event> &event) const;

private:
  VideoDecodeHandler(up<Decoder> &&decoder, const AVRational &r, const sp<Event> &notify) noexcept;
  void HandleEvent(const sp<Event> &event) override;
//    void DecodeFrame(const std::shared_ptr<Event> &event) noexcept;
  sp<AVFrame> DecodePacket(const sp<AVPacket> &video_packet);
  void UpdateSerialNum();

private:
  AVRational stream_time_base_;
  up<Decoder> decoder_;
  sp<Event> notify_;
  sp<EventDispatcher> dispatcher_;
  int64_t serial_num_ = 0;
};

sp<VideoDecodeHandler> CreateVideoDecodeHandler(const std::unordered_map<std::string, std::string> &options,
                                                const AVStream *video_stream,
                                                const sp<Event> &notify);


}

