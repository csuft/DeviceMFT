#pragma once

#include "packet_buffer.h"
#include <av_toolbox/dynamic_circular_buffer.h>
#include <mutex>
#include <algorithm>

namespace ins {

class CircularPacketBuffer : public PacketBuffer {
public:
  CircularPacketBuffer(int capacity);
  void PushPacket(const sp<AVPacket> &video_pkt) override;
  int PopPacket(sp<AVPacket> &video_pkt) override;
  void DiscardUntilDTS(int64_t dts) override;
  void Clear() override;
  uint64_t size() noexcept override;
  uint64_t buffer_duration_in_dts() override;

private:
  std::mutex mtx_;
  dynamic_circular_buffer<sp<AVPacket>> circular_buffer_;
};


}