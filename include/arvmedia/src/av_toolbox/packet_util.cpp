#include "packet_util.h"
#include <llog/llog.h>

namespace ins {

sp<ARVPacket> NewPacket() {
  //  AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket));
  //auto packet = static_cast<ARVPacket*>(malloc(sizeof(ARVPacket)));
  auto packet = static_cast<ARVPacket*>(av_mallocz(sizeof(ARVPacket)));
  CHECK(packet != nullptr) << "malloc AVPacket return nullptr";
  av_packet_unref(packet);
  packet->media_type = AVMEDIA_TYPE_UNKNOWN;
  sp<ARVPacket> sp_pkt(packet, [](AVPacket *pkt) {
    av_packet_free(&pkt);
  });
  return sp_pkt;
}

sp<ARVPacket> NewH264Packet(const uint8_t * data, int size, int64_t pts, int64_t dts,
                            bool keyframe, int stream_index) {
  auto pkt = NewPacket();
  CHECK(pkt != nullptr);
  auto buf = av_buffer_alloc(size);
  CHECK(buf != nullptr);
  pkt->media_type = AVMEDIA_TYPE_VIDEO;
  pkt->buf = buf;
  pkt->data = buf->data;
  pkt->size = size;
  memcpy(pkt->data, data, size);
  pkt->pts = pts;
  pkt->dts = dts;
  pkt->flags = keyframe ? AV_PKT_FLAG_KEY : 0;
  pkt->stream_index = stream_index;
  return pkt;
}

sp<ARVPacket> NewAACPacket(const uint8_t * data, int size, int64_t pts, int stream_index) {
  auto pkt = NewPacket();
  CHECK(pkt != nullptr);
  auto buf = av_buffer_alloc(size);
  CHECK(buf != nullptr);
  pkt->media_type = AVMEDIA_TYPE_AUDIO;
  pkt->buf = buf;
  pkt->data = buf->data;
  pkt->size = size;
  memcpy(pkt->data, data, size);
  pkt->pts = pts;
  pkt->dts = pts;
  pkt->flags = AV_PKT_FLAG_KEY;
  pkt->stream_index = stream_index;
  return pkt;
}

}
