//
// Created by jerett on 16/12/2.
//

#ifndef INSMEDIAUTIL_FFMPEG_OBJ_DEFINE_H
#define INSMEDIAUTIL_FFMPEG_OBJ_DEFINE_H

extern "C" {
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
}
#include "packet_util.h"
#include "frame_util.h"
#include "codec_util.h"
#include "stream_util.h"
#include "filter_util.h"
#include "swr_util.h"
#include "sp.h"
#include "platform.h"

namespace ins {

/***************************util func********************************/
///call many times will ensure init only once
void InitFFmpeg();

std::string FFmpegErrorToString(int code);

inline bool IsKeyFrame(const sp<AVPacket> &pkt) {
  return (pkt->flags & AV_PKT_FLAG_KEY) > 0;
}

inline int64_t TimestampToMs(int64_t pts, const AVRational &r) {
  return av_rescale_q(pts, r, { 1, 1000 });
}

inline int64_t MsToTimestamp(int64_t ms, const AVRational &r) {
  return av_rescale_q(ms, { 1, 1000 }, r);
}

}

#endif //INSMEDIAUTIL_FFMPEG_OBJ_DEFINE_H
