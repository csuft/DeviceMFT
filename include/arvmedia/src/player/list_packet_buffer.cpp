#include "list_packet_buffer.h"
extern "C" {
#include <libavformat/avformat.h>
}
#include "player_error.h"

namespace ins {

void ListPacketBuffer::PushPacket(const sp<AVPacket>& video_pkt) {
  std::lock_guard<std::mutex> lck(mtx_);
  pkts_.push_back(video_pkt);
  buffer_size_ += video_pkt->size;
}

int ListPacketBuffer::PopPacket(sp<AVPacket>& video_pkt) {
  std::lock_guard<std::mutex> lck(mtx_);
  if (pkts_.empty()) {
    return kErrorAgain;
  } else {
    video_pkt = pkts_.front();
    pkts_.pop_front();
    buffer_size_ -= video_pkt->size;
    return kErrorNone;
  }
}

void ListPacketBuffer::DiscardUntilDTS(int64_t dts) {
  std::lock_guard<std::mutex> lck(mtx_);
  while (!pkts_.empty()) {
    if (pkts_.front()->dts < dts) {
      pkts_.pop_front();
    } else {
      break;
    }
  }
}

void ListPacketBuffer::Clear() noexcept {
  std::lock_guard<std::mutex> lck(mtx_);
  pkts_.clear();
  buffer_size_ = 0;
}

uint64_t ListPacketBuffer::buffer_duration_in_dts() {
  std::lock_guard<std::mutex> lck(mtx_);
  if (pkts_.empty()) return 0;
  return pkts_.back()->dts - pkts_.front()->dts;
}

}

