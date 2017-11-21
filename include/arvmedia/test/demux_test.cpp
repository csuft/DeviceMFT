//
// Created by jerett on 16/12/15.
//

#include <gtest/gtest.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/spherical.h>
#include <libavutil/stereo3d.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/avcodec.h>
}
#include <chrono>
#include <llog/llog.h>
#include <av_toolbox/demuxer.h>
#include <av_toolbox/decoder.h>
#include <av_toolbox/encoder.h>
#include <av_toolbox/muxer.h>
#include <av_toolbox/scaler.h>

#include <av_toolbox/bsf_filter.h>
#include <av_toolbox/ffmpeg_util.h>

using namespace ins;

TEST(av, demux) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  {
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/shenda.insv"));
    //demuxer.reset(new Demuxer("C:/Users/wjjia/Desktop/aaa.mp4"));
    CHECK(demuxer->Open(nullptr) == 0);
  }

  LOG(INFO) << "video duration:" << demuxer->video_duration()
    << " audio duration:" << demuxer->audio_duration()
    << " duration:" << demuxer->duration();

  auto vs = demuxer->video_stream();
  if (vs) {
    {
      auto spherical = av_stream_get_side_data(const_cast<AVStream*>(vs), AV_PKT_DATA_SPHERICAL, nullptr);
      if (spherical != nullptr) {
        AVSphericalMapping *mapping = reinterpret_cast<AVSphericalMapping*>(spherical);
        if (mapping->projection == AV_SPHERICAL_CUBEMAP) {
          LOG(INFO) << "cube map";
        } else {
          LOG(INFO) << "equirectangular map";
        }
      } else {
        LOG(INFO) << "no spherical pkt data.";
      }
    }
    
    {
      auto stereo = av_stream_get_side_data(const_cast<AVStream*>(vs), AV_PKT_DATA_STEREO3D, nullptr);
      if (stereo != nullptr) {
        auto stereo_3d = reinterpret_cast<AVStereo3D*>(stereo);
        LOG(INFO) << "spherical video.";
        LOG(INFO) << "type:" << av_stereo3d_type_name(stereo_3d->type);
      } else {
        LOG(INFO) << "no stereo3d pkt data";
      }
    }
  }

//  LOG(INFO) << "firt dts:" << demuxer->video_stream()->first_dts;
  sp<ARVPacket> pkt;
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      LOG(INFO) << "here....";
//      LOG(INFO) << "video pkt:"
//                << " pts" << pkt->pts
//                << " dts" << pkt->dts
//                << " is_key:" << pkt->flags;
//      LOG(INFO) << "cur dts:" << demuxer->video_stream()->cur_dts
//                << " first dts:" << demuxer->video_stream()->first_dts;
    } else if (pkt->stream_index == demuxer->audio_stream_index()) {
//      LOG(INFO) << "audio pkt:" << TimestampToMs(pkt->pts, demuxer->audio_stream()->time_base);
    }
  }
  LOG(INFO) << "finish.";
}

TEST(av, demux_filter) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  sp<BSFFilter> filter;
  {
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/shenda.insv"));
    //demuxer.reset(new Demuxer("C:\\Users\\wjjia\\Desktop\\VID_20170322_143654_141.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
  }
  LOG(INFO) << "video duration:" << demuxer->video_duration()
    << " audio duration:" << demuxer->audio_duration()
    << " duration:" << demuxer->duration();

  while (true) {
    sp<ARVPacket> pkt;
    auto ret = demuxer->NextPacket(pkt);
     
    if (ret == 0) {
      if (pkt->stream_index == demuxer->video_stream_index()) {
        for (int i = 0; i < 5; ++i) {
          std::printf("%0x ", pkt->data[i]);
        }
        std::printf("\n");
      }
    } else if (ret == AVERROR_EOF) {
      LOG(INFO) << "finish.";
      break;
    } else {
      LOG(ERROR) << "read err:" << FFmpegErrorToString(ret);
      break;
    }
  }
}
