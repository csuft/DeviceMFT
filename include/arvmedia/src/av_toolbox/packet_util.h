#pragma once

extern "C" {
#include <libavformat/avformat.h>
};
#include "sp.h"

namespace ins {

struct ARVPacket : public AVPacket {
  AVMediaType media_type;
};

//sp<AVPacket> NewAVPacket();
sp<ARVPacket> NewPacket();
sp<ARVPacket> NewH264Packet(const uint8_t *data, int size, int64_t pts, int64_t dts, bool keyframe, int stream_index);
sp<ARVPacket> NewAACPacket(const uint8_t *data, int size, int64_t pts, int stream_index);

}
