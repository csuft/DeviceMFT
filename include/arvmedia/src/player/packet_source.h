//
//  packet_source.h
//
//  Created by jerett on 16/2/16.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef packet_source_h
#define packet_source_h

#include <memory>
#include <string>
#include <atomic>
#include <unordered_map>

#include <eventloop/event_handler.h>
#include <llog/llog.h>
#include <av_toolbox/ffmpeg_util.h>

#include "packet_buffer.h"

namespace ins {

class Demuxer;
class Event;
class EventDispatcher;

class PacketSource : public EventHandler, public std::enable_shared_from_this<PacketSource> {
  friend sp<PacketSource> NewSource(const std::string &url, const sp<Event> &notify);
  using BufferingProgressCallback = std::function<void(double)>;

public:
  enum {
    kPrepared = 2000,
    kError,
  };

  ~PacketSource();
  PacketSource(const PacketSource&) = delete;
  PacketSource& operator=(const PacketSource&) = delete;
  void StartAsync();
  void ReleaseSync();

  /*!
    \brief Send prepare event
    \param options Support buffer_ms:int, max_buffer_size:int 
  */
  void PrepareAsync(const std::unordered_map<std::string, std::string> &options);
  /*!
    \brief Source seek to the target dts.
    \return Seek result. reference Demuxer seek return value.
  */
  int SeekSync(int stream_index, int64_t dts, int flags);
  /*!
    \brief BufferSync
    \param progress_callback Buffer progress callback
  */
  void BufferSync(const BufferingProgressCallback &progress_callback) ;
  /*!
    \brief if On buffering, break buffer.
  */
  void BreakBuffer() {
    to_release_ = true;
  }
  /*
   * \brief Atempt read video pkt from buffer.
   * \param video_packet
   * \return kErrorNone:no error occur. kErrorEnd:no data. kErrorAgain:read again.
  */
  int ReadVideo(sp<AVPacket> &video_packet);
  /*
  * \brief Atempt read audio pkt from buffer.
  * \param audio_packet
  * \return kErrorNone:no error occur. kErrorEnd:no data. kErrorAgain:read again.
  */
  int ReadAudio(sp<AVPacket> &audio_packet);
  /*
  * \brief Continue read unitl video pkt.
  * \param video_pkt
  * \return kErrorNone:no error occur. kErrorEnd:no data..
  */
  int ReadUntilVideoSync(sp<AVPacket> &video_pkt);

  void ClearVideoSync();

private:
  PacketSource(const std::string &url,
               const sp<Event> &notify);

  void HandleEvent(const sp<Event> &event) override;
  void Prepare(const sp<ins::Event> &event);
  void ReadSource();
  int ReadUntilVideo(sp<AVPacket> &video_packet);
  void Seek(const sp<Event> &event);
  void Buffer(const std::function<void(double)> &progress_callback);
  int64_t BufferDurationInMs() const;

  bool BufferIsFull() const {
    return video_buffer_->size() + audio_buffer_->size() > kMaxBufferSize;
  }

private:
  std::string url_;
  up<Demuxer> demuxer_;
  sp<Event> notify_;
  sp<EventDispatcher> dispatcher_;

  bool wait_for_key_frame_ = false;
  bool eof_ = false;
  bool to_release_ = false;

  up<PacketBuffer> video_buffer_;
  up<PacketBuffer> audio_buffer_;

  /**
   *  only used when playing network video
   */
  int kBufferDurationInMs = 5*1000;
  int kMaxBufferSize = 20*1024*1024;
};

sp<PacketSource> NewSource(const std::string &url, const sp<Event> &notify);


}




#endif /* packet_source_h */
