//
// Created by jerett on 16/7/14.
//

#ifndef INSPLAYER_MUX_SINK_H
#define INSPLAYER_MUX_SINK_H

extern "C" {
#include <libavutil/rational.h>
}
#include <av_toolbox/muxer.h>
#include "media_filter.h"

namespace ins {

class MuxSink: public PacketSink {
public:
  explicit MuxSink(const std::string &format, const std::string &file_url);
  ~MuxSink() = default;
  
  /**
   * \brief Set whether output fragment mp4, it works when output format is mp4
   * \param fragment_mp4 Decide whether fragment mp4
   * \param frames_per_fragment 
   * \return Self for chain call
   */
  MuxSink* set_fragment_mp4(bool fragment_mp4, int frames_per_fragment) {
    fragment_mp4_ = fragment_mp4;
    frames_per_fragment_ = frames_per_fragment;
    return this;
  }
  /**
   * \brief Set spherical projection
   * \param spherical_type Must be equirectangular or cubemap(not support now)
   * \return Self for chain call
   */
  MuxSink* set_spherical_projection(const std::string &spherical_type) {
    spherical_type_ = spherical_type;
    return this;
  }
  /**
   * \brief Set stereo type
   * \param stereo_type Must be mono, left-right, or top-bottom.
   * \return Self for chain call
   */
  MuxSink* set_stereo_type(const std::string &stereo_type) {
    stereo_type_ = stereo_type;
    return this;
  }
  /**
   *\brief Set spatial audio
   * @param spatial_audio True if spatial audio
   * @return Self for chain call
   */
  MuxSink* set_spatial_audio(bool spatial_audio) {
    spatial_audio_ = spatial_audio;
    return this;
  }

  bool Init(MediaContext *ctx) override;
  bool Filter(MediaContext *ctx, const sp<ARVPacket> &data) override;
  void Close(MediaContext *ctx) override;
  void Interrupt();
  /**
   * \brief Will automatically call if not called manually when first call Filter.
   * \return Whther open success.
   */
  bool Open();

private:
  bool ConfigVideoStream(const sp<AVStream> &vst);
  bool ConfigAudioStream(const sp<AVStream> &ast);

private:
  AVRational src_video_timebase_;
  AVRational src_audio_timebase_;
  bool fragment_mp4_ = false;
  int frames_per_fragment_ = INT_MAX;
  int fragment_counter_ = 0;
  std::string spherical_type_;
  std::string stereo_type_;
  bool spatial_audio_ = false;

  bool is_open_ = false;
  std::string file_url_;
  up<Muxer> muxer_;
};

}


#endif //INSPLAYER_MUX_SINK_H
