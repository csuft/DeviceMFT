#include "circular_packet_buffer.h"
extern "C" {
#include <libavformat/avformat.h>
}
#include "player_error.h"

namespace ins {

CircularPacketBuffer::CircularPacketBuffer(int capacity) : circular_buffer_(capacity) {
  ;
}

uint64_t CircularPacketBuffer::size() noexcept {
  std::lock_guard<std::mutex> lck(mtx_);
  uint64_t size = 0;
  std::for_each(circular_buffer_.begin(), circular_buffer_.end(), [&size](const sp<AVPacket> &pkt) {
    size += pkt->size;
  });
  return size;
}

void CircularPacketBuffer::PushPacket(const sp<AVPacket> &video_pkt) {
  std::lock_guard<std::mutex> lck(mtx_);
  circular_buffer_.push_back(video_pkt);
}

int CircularPacketBuffer::PopPacket(sp<AVPacket> &video_pkt) {
  std::lock_guard<std::mutex> lck(mtx_);
  if (circular_buffer_.empty()) {
    return kErrorAgain;
  } else {
    video_pkt = circular_buffer_.front();
    circular_buffer_.pop_front();
    return kErrorNone;
  }
}

void CircularPacketBuffer::DiscardUntilDTS(int64_t dts) {
  std::lock_guard<std::mutex> lck(mtx_);
  while (!circular_buffer_.empty()) {
    if (circular_buffer_.front()->dts < dts) {
      circular_buffer_.pop_front();
    } else {
      break;
    }
  }
}

void CircularPacketBuffer::Clear() {
  std::lock_guard<std::mutex> lck(mtx_);
  circular_buffer_.clear();
}

uint64_t CircularPacketBuffer::buffer_duration_in_dts() {
  std::lock_guard<std::mutex> lck(mtx_);
  if (circular_buffer_.empty()) return 0;
  return circular_buffer_.back()->dts - circular_buffer_.front()->dts;
}

}