#include <gtest/gtest.h>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
}
#include <av_toolbox/demuxer.h>
#include <av_toolbox/decoder.h>
#include <av_toolbox/scaler.h>
#include <av_toolbox/ffmpeg_util.h>
#include <llog/llog.h>

using namespace ins;

TEST(scale, video) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  {
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/game-test.mp4"));
    CHECK(demuxer->Open(nullptr) == 0);
    //    CHECK(demuxer->video_stream_index() >= 0 &&
    //        demuxer->video_stream()->codecpar->codec_id == AV_CODEC_ID_H264);
    //    CHECK(demuxer->audio_stream_index() >= 0 &&
    //        demuxer->audio_stream()->codecpar->codec_id == AV_CODEC_ID_AAC);
  }

  auto vst = demuxer->video_stream();
  auto dec_ctx = NewAVHwaccelDecodeContextFromID(vst->codecpar->codec_id, true);
  avcodec_parameters_to_context(dec_ctx.get(), vst->codecpar);
  Decoder video_decoder(dec_ctx);
  CHECK(video_decoder.Open() == 0);

  sp<ARVPacket> pkt;
  sp<AVFrame> video_frame;
  Scaler scaler(1920, 960, AV_PIX_FMT_RGBA, SWS_BICUBIC);
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      CHECK(video_decoder.SendPacket(pkt) == 0);
      if (video_decoder.ReceiveFrame(video_frame) == 0) {
        CHECK(scaler.Init(video_frame));
        sp<AVFrame> scale_frame;
        auto ret = scaler.ScaleFrame(video_frame, scale_frame);
        CHECK(scale_frame != nullptr) << FFmpegErrorToString(ret);
        //        LOG(INFO) << "video decode:" << TimestampToMs(pkt->pts, demuxer->video_stream()->time_base);
        LOG(VERBOSE) << "video decode:" << TimestampToMs(scale_frame->pts, demuxer->video_stream()->time_base)
          << " line_size:" << scale_frame->linesize[0]
          << " line_size2:" << scale_frame->linesize[1]
          << " width:" << scale_frame->width
          << " height:" << scale_frame->height
          << " pix_fmt:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(scale_frame->format))->name;
      }
    }
  }
  LOG(INFO) << "finish.";
}
