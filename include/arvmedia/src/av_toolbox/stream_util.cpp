#include "stream_util.h"
#include <llog/llog.h>

namespace ins {

sp<AVFormatContext> NewOutputAVFormatContext(const std::string &format_name, const std::string &filename) {
  AVFormatContext *fmt_ctx = nullptr;
  auto ret = avformat_alloc_output_context2(&fmt_ctx, nullptr, format_name.c_str(), filename.c_str());
  CHECK(ret >= 0);
  sp<AVFormatContext> sp_fmt_ctx(fmt_ctx, [](AVFormatContext *s) {
    avformat_free_context(s);
  });
  return sp_fmt_ctx;
}

sp<AVStream> NewAVStream() {
  auto st = reinterpret_cast<AVStream*>(av_malloc(sizeof(AVStream)));
  st->codecpar = avcodec_parameters_alloc();
  CHECK(st != nullptr);
  sp<AVStream> sp_s(st, [](AVStream *stream) {
    avcodec_parameters_free(&stream->codecpar);
    av_freep(&stream);
  });
  return sp_s;
}

sp<AVStream> AddAVStream(const sp<AVFormatContext> &sp_fmt) {
  auto s = avformat_new_stream(sp_fmt.get(), nullptr);
  sp<AVStream> sp_s(s, [](AVStream *stream) {
  });
  return sp_s;
}

sp<AVStream> NewVideoStream(int width, int height,
                            AVRational framerate, AVRational timebase,
                            AVCodecID codec_id, AVPixelFormat pixel_fmt) {
  auto vst = NewAVStream();
  vst->codecpar->width = width;
  vst->codecpar->height = height;
  vst->codecpar->format = pixel_fmt;
  vst->codecpar->codec_id = codec_id;
  vst->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  vst->codecpar->codec_tag = 0;
  vst->avg_frame_rate = framerate;
  vst->time_base = timebase;
  return vst;
}

sp<AVStream> NewH264Stream(int width, int height,
                           AVRational framerate, AVRational timebase,
                           AVCodecID codec_id, AVPixelFormat pixel_fmt,
                           const uint8_t *extradata, int extradata_size) {
  auto vst = NewVideoStream(width, height, framerate, timebase, codec_id, pixel_fmt);
  if (!vst) return nullptr;
  vst->codecpar->extradata = reinterpret_cast<uint8_t*>(av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
  vst->codecpar->extradata_size = extradata_size;
  memcpy(vst->codecpar->extradata, extradata, extradata_size);
  return vst;
}

sp<AVStream> NewAACStream(AVSampleFormat sample_fmt,
                          uint64_t channel_layout, int channels, int sample_rate,
                          uint8_t *extradata, int extradata_size,
                          AVRational timebase) {
  auto aac_stream = NewAVStream();
  aac_stream->codecpar->format = sample_fmt;
  aac_stream->codecpar->channel_layout = channel_layout;
  aac_stream->codecpar->channels = channels;
  aac_stream->codecpar->sample_rate = sample_rate;
  aac_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
  aac_stream->codecpar->codec_id = AV_CODEC_ID_AAC;
  aac_stream->codecpar->extradata = reinterpret_cast<uint8_t*>(av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
  aac_stream->codecpar->extradata_size = extradata_size;
  //LOG(VERBOSE) << "extradata size:" << extradata_size;
  aac_stream->codecpar->frame_size = 1024;
  aac_stream->time_base = timebase;
  memcpy(aac_stream->codecpar->extradata, extradata, extradata_size);
  return aac_stream;
}

}
