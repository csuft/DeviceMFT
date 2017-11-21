#pragma once

#include <av_toolbox/sp.h>

struct AVPacket;

namespace ins {

class PacketBuffer {
public:
  PacketBuffer() = default;
  virtual ~PacketBuffer() = default;
  PacketBuffer(const PacketBuffer&) = delete;
  PacketBuffer& operator=(const PacketBuffer&) = delete;

  virtual void PushPacket(const sp<AVPacket> &video_pkt) = 0;
  virtual int PopPacket(sp<AVPacket> &audio_pkt) = 0;
  virtual void DiscardUntilDTS(int64_t dts) = 0;
  virtual void Clear() = 0;
  virtual uint64_t size() = 0;
  virtual uint64_t buffer_duration_in_dts() = 0;
};

}