
#include "bsf_filter.h"

extern "C" {
#include <libavcodec/avcodec.h>
}
#include <llog/llog.h>

namespace ins {

int BSFFilter::Open(const std::string &name, const AVStream *st) {
  bsf_ctx_ = NewAVBSFContext(name, st);
  if (bsf_ctx_ == nullptr) return -1;
  auto ret = av_bsf_init(bsf_ctx_.get());
  if (ret < 0) {
    LOG(ERROR) << "bsf_init failed:" << ret <<  FFmpegErrorToString(ret);
  }
  LOG(ERROR) << "bsf_init ret:" << ret;
  return ret;
}

int BSFFilter::SendPacket(const sp<ARVPacket>& pkt) {
  auto pkt_c = pkt ? pkt.get() : nullptr;
  return av_bsf_send_packet(bsf_ctx_.get(), pkt_c);
}

int BSFFilter::ReceivePacket(sp<ARVPacket>& pkt) {
  return av_bsf_receive_packet(bsf_ctx_.get(), pkt.get());
}

int BSFFilter::Flush() {
  return av_bsf_init(bsf_ctx_.get());
}


}
