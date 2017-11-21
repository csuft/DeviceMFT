#pragma once

#include "packet_buffer.h"
#include <list>
#include <mutex>

namespace ins {

class ListPacketBuffer : public PacketBuffer {
public:
  void PushPacket(const sp<AVPacket> &video_pkt) override;
  int PopPacket(sp<AVPacket> &video_pkt) override;
  void DiscardUntilDTS(int64_t dts) override;
  void Clear() noexcept override;
  
  uint64_t size() noexcept override {
    std::lock_guard<std::mutex> lck(mtx_);
    return buffer_size_;
  }
  uint64_t buffer_duration_in_dts() override;

private:
  uint64_t buffer_size_ = 0;
  std::mutex mtx_;
  std::list<sp<AVPacket>> pkts_;
};

}



