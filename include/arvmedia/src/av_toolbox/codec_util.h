#pragma once
#include "platform.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <string>
#include "sp.h"

namespace ins {

sp<AVCodecParameters> NewAVCodecParameters();
sp<AVCodecContext> NewAVCodecContext(AVCodec*);

/***************************decode context func********************************/
sp<AVCodecContext> NewAVDecodeContextFromID(AVCodecID id);
sp<AVCodecContext> NewAVDecodeContextFromName(const std::string &name);
sp<AVCodecContext> NewAVHwaccelDecodeContextFromID(AVCodecID id, bool copy_hwaccel = false);

/***************************encode context func********************************/
sp<AVCodecContext> NewAVEncodeContextFromID(AVCodecID id);
sp<AVCodecContext> NewAVHwaccelEncodeContextFromID(AVCodecID id);
sp<AVCodecContext> NewAVEncodeContextFromName(const std::string &name);


}
