//
// Created by jerett on 16/12/23.
//

#ifndef INSPLAYER_SCALE_FILTER_H
#define INSPLAYER_SCALE_FILTER_H

#include "av_toolbox/scaler.h"
#include "editor/editor_error.h"
#include "media_filter.h"

namespace ins {

class ScaleFilter : public VideoImageFilter<sp<AVFrame>> {
public:
  ///
  /// \param out_width  Scale out width, will equal to in_width if set -1
  /// \param out_height Scale out height, will equal to in_height if set -1
  /// \param out_fmt  Scale out pix fmt
  /// \param algorithm scale alogrithm, recommand using SWS_FAST_BILINEAR, 
  /// ref:<libswscale/swscale.h> or http://blog.csdn.net/leixiaohua1020/article/details/12029505
  ScaleFilter(int out_width, int out_height, AVPixelFormat out_fmt, int algorithm)
    :out_width_(out_width), out_height_(out_height), out_fmt_(out_fmt), algorithm_(algorithm) {
  }
  ~ScaleFilter() = default;

  bool Init(MediaContext *ctx) override {
    if (out_width_ == -1) out_width_ = ctx->output_stream.codecpar->width;
    if (out_height_ == -1) out_height_ = ctx->output_stream.codecpar->height;
    ctx->output_stream.codecpar->width = out_width_;
    ctx->output_stream.codecpar->height = out_height_;
    ctx->output_stream.codecpar->format = out_fmt_;
    scaler_ = std::make_shared<Scaler>(out_width_, out_height_, out_fmt_, algorithm_);
    sp<AVFrame> out_frame;
    return this->next_filter_->Init(ctx);
  }

  bool Filter(MediaContext *ctx, const sp<AVFrame> &data) override {
    if (!scaler_->Init(data)) return false;
    sp<AVFrame> scale_frame;
    auto ret = scaler_->ScaleFrame(data, scale_frame);
    if (ret <= 0) {
      ctx->error_code = kScaleError;
      return false;
    }
    return next_filter_->Filter(ctx, scale_frame);
  }

  void Close(MediaContext *ctx) override {
    return next_filter_->Close(ctx);
  }

private:
  bool Test(const sp<AVFrame> &test_frame, sp<AVFrame> &out_frame) {
    if (test_frame == nullptr) return true;
    if (!scaler_->Init(test_frame)) return false;
    auto ret = scaler_->ScaleFrame(test_frame, out_frame);
    return ret > 0;
  }

private:
  int out_width_;
  int out_height_;
  AVPixelFormat out_fmt_;
  int algorithm_;
  sp<Scaler> scaler_;
};

}

#endif //INSPLAYER_SCALE_FILTER_H
