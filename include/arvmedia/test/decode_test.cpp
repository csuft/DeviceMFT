#include <gtest/gtest.h>
extern "C" {
#include <libavutil/pixdesc.h>
}
#include <bitset>
#include <av_toolbox/demuxer.h>
#include <av_toolbox/decoder.h>
#include <av_toolbox/ffmpeg_util.h>
#include <llog/llog.h>

using namespace ins;

TEST(decode, cpu) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  {
    #if (_WIN32)
    demuxer.reset(new Demuxer("D:/Videos/pro/41.mp4"));
    //    demuxer.reset(new Demuxer("/Users/jerett/Desktop/shenda.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
    #elif (__APPLE__)
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/shenda.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
    #endif
  }

  auto vst = demuxer->video_stream();
  //cpu config
  auto dec_ctx = NewAVDecodeContextFromID(vst->codecpar->codec_id);
  dec_ctx->thread_count = 4;
  avcodec_parameters_to_context(dec_ctx.get(), vst->codecpar);
  Decoder video_decoder(dec_ctx);
  CHECK(video_decoder.Open() == 0);

  sp<ARVPacket> pkt;
  sp<AVFrame> video_frame;

  ins::Timer fps_timer;
  auto frame_cnt = 0;
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      CHECK(video_decoder.SendPacket(pkt) == 0);
      if (video_decoder.ReceiveFrame(video_frame) == 0) {
        LOG(INFO) << "video decode:" << TimestampToMs(pkt->pts, demuxer->video_stream()->time_base)
          << " type:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(video_frame->format))->name;
        ++frame_cnt;
        if (fps_timer.Pass() >= 1000) {
          auto fps = frame_cnt * 1000 / fps_timer.Pass();
          LOG(INFO) << "fps:" << fps;
          frame_cnt = 0;
          fps_timer.Reset();
        }
      }
    }
  }

  video_decoder.SendEOF();
  while (video_decoder.ReceiveFrame(video_frame) != AVERROR_EOF) {
    LOG(INFO) << "flush video "
      << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
      << " frame type:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(video_frame->format))->name;
  }
  LOG(INFO) << "finish.";
}

TEST(decode, hwaccel) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  {
#if (_WIN32)
    //demuxer.reset(new Demuxer("D:/Videos/pro/41.mp4"));
    demuxer.reset(new Demuxer("D:/Videos/pro/contrast/origin_5.mp4"));
    //demuxer.reset(new Demuxer("/Users/jerett/Desktop/shenda.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
#elif (__APPLE__)
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/shenda.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
#endif
  }

  auto vst = demuxer->video_stream();
  auto dec_ctx = NewAVHwaccelDecodeContextFromID(vst->codecpar->codec_id);
  avcodec_parameters_to_context(dec_ctx.get(), vst->codecpar);
  Decoder video_decoder(dec_ctx);
  CHECK(video_decoder.Open() == 0);

  sp<ARVPacket> pkt;
  sp<AVFrame> video_frame;

  ins::Timer fps_timer;
  auto frame_cnt = 0;
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      CHECK(video_decoder.SendPacket(pkt) == 0);
      if (video_decoder.ReceiveFrame(video_frame) == 0) {
        LOG(INFO) << "video decode:" << TimestampToMs(pkt->pts, demuxer->video_stream()->time_base)
          << " type:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(video_frame->format))->name;
        ++frame_cnt;
        if (fps_timer.Pass() >= 1000) {
          auto fps = frame_cnt * 1000 / fps_timer.Pass();
          LOG(INFO) << "fps:" << fps;
          frame_cnt = 0;
          fps_timer.Reset();
        }
      }
    }
  }

  video_decoder.SendEOF();
  while (video_decoder.ReceiveFrame(video_frame) != AVERROR_EOF) {
    LOG(INFO) << "flush video "
      << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
      << " frame type:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(video_frame->format))->name;
  }
  LOG(INFO) << "finish.";
}

