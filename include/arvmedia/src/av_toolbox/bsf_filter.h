#pragma once

#include <string>
#include "ffmpeg_util.h"

struct AVBSFContext;
struct AVBitStreamFilter;
struct AVStream;

namespace ins {

class BSFFilter {
public:
  BSFFilter() noexcept = default;

  int Open(const std::string &name, const AVStream *st);
  int SendPacket(const sp<ARVPacket> &pkt);
  int ReceivePacket(sp<ARVPacket> &pkt);
  //just reinit filter
  int Flush();

  sp<const AVBSFContext> bsf_ctx() const {
    return bsf_ctx_;
  }

private:
  sp<AVBSFContext> bsf_ctx_;
};

}