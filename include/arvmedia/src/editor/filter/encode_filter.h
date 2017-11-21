//
// Created by jerett on 16/8/11.
//

#ifndef INSPLAYER_ENCODE_FILTER_H
#define INSPLAYER_ENCODE_FILTER_H

extern "C" {
#include <libavcodec/avcodec.h>
}
#include <string>
#include <llog/llog.h>
#include <memory>
#include "av_toolbox/sp.h"
#include "av_toolbox/encoder.h"
#include "media_filter.h"

struct AVFrame;
struct AVPacket;

///test video encode only now.
namespace ins {

class EncodeFilter : public VideoImageFilter<sp<ARVPacket>> {
public:
  explicit EncodeFilter(AVCodecID encodec_id) noexcept : encodec_id_(encodec_id) {}
  ~EncodeFilter() = default;
  /**
   * \brief Set encode bitrate.
   * \param bitrate H264 bitrate
   * \return Self
   */
  EncodeFilter* set_bitrate(int bitrate) {
    bitrate_ = bitrate;
    return this;
  }
  /**
   * \brief Set bframes number.
   * \param bframes B frames number
   * \return Self
   */
  EncodeFilter* set_bframes(int bframes) {
    bframes_ = bframes;
    return this;
  }
  /**
   * \brief Set encode gop size. value type:int
   * \param gop_size H264 gop size.
   * \return Self
   */
  EncodeFilter* set_gopsize(int gop_size) {
    gop_size_ = gop_size;
    return this;
  }
  /**
   * \brief Set encode threads. value type: int
   * \param threads Encode threads num
   * \return Self
   */
  EncodeFilter* set_threads(int threads) {
    threads_ = threads;
    return this;
  }
  /**
   * \brief Set encode preset
   * \param preset Reference ffmpeg document.
   * \return Self
   */
  EncodeFilter* set_preset(const std::string &preset) {
    preset_ = preset;
    return this;
  }
  /**
   * \brief Set encode tune.
   * \param tune Reference ffmpeg document.
   * \return Self
   */
  EncodeFilter* set_tune(const std::string &tune) {
    tune_ = tune;
    return this;
  }
  /**
   * \brief Indicate whether use haradware encode, default is true
   * \param enable_hwaccel 
   * \return Self
   */
  EncodeFilter* set_enable_hwaccel(bool enable_hwaccel) {
    enable_hwaccel_ = enable_hwaccel;
    return this;
  }
  /**
   * \brief Setting whether videotoolbox encode in realtime mode
   * \param realtime 
   * \return Self
   */
  EncodeFilter* set_videotoolbox_realtime(bool realtime) {
    videotoolbox_realtime_ = realtime;
    return this;
  }

  bool enable_hwaccel() const {
    return enable_hwaccel_;
  }

  bool Init(MediaContext *ctx) override;
  bool Filter(MediaContext *ctx, const sp<AVFrame> &data) override;
  void Notify(MediaContext *ctx, const MediaNotify &notify) override;
  void Close(MediaContext *ctx) override;

private:
  bool InitHwaccelEncoder(MediaContext *ctx);
  bool InitSoftwareEncoder(MediaContext *ctx);
  bool ConfigureEncContext(MediaContext *ctx);
  bool Test(MediaContext *ctx, const sp<AVFrame> &test_frame, sp<ARVPacket> &out_test_pkt);
  bool Reset(MediaContext *ctx);
  sp<AVFrame> PreprocessInputFrame(const sp<AVFrame> &in_data) const;

private:
  int stream_index_ = -1;
  int bitrate_ = -1;
  int bframes_ = -1;
  int gop_size_ = -1;
  int threads_ = -1;
  bool enable_hwaccel_ = true;
  bool videotoolbox_realtime_ = true;

  std::string preset_;
  std::string tune_;
  AVCodecID encodec_id_;
  sp<AVCodecContext> enc_ctx_;
  up<Encoder> encoder_;
  enum EncoderType {
    kEncoderX264,
    kEncoderX265,
    kEncoderH264NVENC,
    kEncoderH265NVENC,
    kEncoderVideoToolbox
  };
  int encoder_type_ = -1;
};

}


#endif //INSPLAYER_X264_ENCODE_FILTER_H
