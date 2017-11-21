//
// Created by jerett on 16/7/18.
//

#ifndef INSPLAYER_DECODE_FILTER_H
#define INSPLAYER_DECODE_FILTER_H

#include <memory>
#include <string>

extern "C" {
#include <libavutil/rational.h>
}
#include "media_filter.h"
#include <av_toolbox/decoder.h>

struct AVCodecContext;

namespace ins {

class DecodeFilter : public StreamFilter<sp<AVFrame>> {
public:
  DecodeFilter() noexcept = default;
  ~DecodeFilter() = default;
  //indicate whether use haradware decode. default is true
  DecodeFilter* set_enable_hwaccel(bool enable_hwaccel) {
    enable_hwaccel_ = enable_hwaccel;
    return this;
  }
  //set whether copy hwaccel gpu content to CPU memory.
  //some platfrom may not effect like VideoToolbox. users should judge by AVFrame.pix_fmt, default is false
  DecodeFilter* set_copy_hwaccel(bool copy_hwaccel) {
    copy_hwaccel_ = copy_hwaccel;
    return this;
  }
  //set decode threads num, only use in cpu decode. default is -1
  DecodeFilter* set_threads(int threads) {
    threads_ = threads;
    return this;
  }

  bool Init(MediaContext *bus) override;
  bool Filter(MediaContext *ctx, const sp<ARVPacket> &data) override;
  void Close(MediaContext *ctx) override;
  void Notify(MediaContext *ctx, const MediaNotify &notify) override;

  bool enable_hwaccel() const {
    return enable_hwaccel_;
  }
 
private:
  bool InitHardwareDecoder(MediaContext *ctx);
  bool InitSoftwareDecoder(MediaContext *ctx);
  bool Test(const sp<ARVPacket> &pkt, sp<AVFrame> &out_test_frame);

private:
  bool enable_hwaccel_ = true;
  bool copy_hwaccel_ = false;
  int threads_ = -1;
  AVRational time_base_;
//  AVStream *stream_ = nullptr;
  sp<AVCodecContext> dec_ctx_;
  up<Decoder> decoder_ = nullptr;
};

}


#endif //INSPLAYER_DECODE_FILTER_H
