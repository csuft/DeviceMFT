#pragma once
extern "C" {
#include <libavformat/avformat.h>
}

#include <string>
#include "sp.h"

namespace ins {

sp<AVFormatContext> NewOutputAVFormatContext(const std::string &format_name, const std::string &filename);

sp<AVStream> NewAVStream();
sp<AVStream> AddAVStream(const sp<AVFormatContext> &sp_fmt);
sp<AVStream> NewVideoStream(int width, int height,
                            AVRational framerate, AVRational timebase,
                            AVCodecID codec_id, AVPixelFormat pixel_fmt);
sp<AVStream> NewH264Stream(int width, int height,
                           AVRational framerate, AVRational timebase,
                           AVCodecID codec_id, AVPixelFormat pixel_fmt,
                           const uint8_t *extradata, int extradata_size);
sp<AVStream> NewAACStream(AVSampleFormat sample_fmt,
                          uint64_t channel_layout, int channels, int sample_rate,
                          uint8_t *extradata, int extradata_size,
                          AVRational timebase);

}
