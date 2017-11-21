#include <gtest/gtest.h>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
}
#include <av_toolbox/demuxer.h>
#include <av_toolbox/decoder.h>
#include <av_toolbox/scaler.h>
#include <av_toolbox/ffmpeg_util.h>
#include <av_toolbox/encoder.h>
#include <av_toolbox/muxer.h>
#include <llog/llog.h>

using namespace ins;

TEST(transcode, video) {
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
    auto video_duration = TimestampToMs(demuxer->video_stream()->duration, demuxer->video_stream()->time_base);
    auto audio_duration = TimestampToMs(demuxer->audio_stream()->duration, demuxer->audio_stream()->time_base);
    LOG(INFO) << "video duration:" << video_duration;
    LOG(INFO) << "audio duration:" << audio_duration;
  }

  auto vst = demuxer->video_stream();
  auto dec_ctx = NewAVHwaccelDecodeContextFromID(vst->codecpar->codec_id);
  avcodec_parameters_to_context(dec_ctx.get(), vst->codecpar);
  sp<Decoder> video_decoder(new Decoder(dec_ctx));
  CHECK(video_decoder->Open() == 0);

  auto enc_ctx = NewAVHwaccelEncodeContextFromID(vst->codecpar->codec_id);
  avcodec_parameters_to_context(enc_ctx.get(), vst->codecpar);
  enc_ctx->codec_tag = 0;
  enc_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
  sp<Encoder> video_encoder(new Encoder(enc_ctx));
  CHECK(video_encoder->Open() == 0);

  LOG(INFO) << "encode extradata size:" << video_encoder->enc_ctx()->extradata_size;
  //  for (int i = 0; i < video_encoder->enc_ctx()->extradata_size; ++i) {
  //    printf("%0x ", video_encoder->enc_ctx()->extradata[i]);
  //  }
  //  printf("\n");

  sp<Muxer> muxer;
  {
#if __APPLE__
    auto out_fmt = NewOutputAVFormatContext("mp4", "/Users/jerett/Desktop/fuck.mp4");
#elif _WIN32
    auto out_fmt = NewOutputAVFormatContext("mp4", "C:/Users/wjjia/Desktop/av-transcode.mp4");
#else
    exit(-1);
#endif // __APPLE__
    muxer.reset(new Muxer(out_fmt));

    auto video_stream = muxer->GetVideoStream();
    avcodec_parameters_from_context(video_stream->codecpar, video_encoder->enc_ctx().get());
    video_stream->time_base = demuxer->video_stream()->time_base;
    video_stream->codecpar->codec_tag = 0;
    CHECK(muxer->Open() == 0);
  }

  sp<AVFrame> video_frame;
  sp<AVFrame> audio_frame;
  sp<ARVPacket> h264_pkt;
  sp<ARVPacket> pkt;
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      CHECK(video_decoder->SendPacket(pkt) == 0);
      if (video_decoder->ReceiveFrame(video_frame) == 0) {
        LOG(INFO) << "decode video,"
          << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
          << " frame type:" << av_get_picture_type_char(video_frame->pict_type);
        CHECK(video_encoder->SendFrame(video_frame) == 0);
        if (video_encoder->ReceivePacket(h264_pkt) == 0) {
          LOG(INFO) << "encode h264."
            << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
            << " size:" << h264_pkt->size;
          muxer->WriteVideoPacket(h264_pkt, demuxer->video_stream()->time_base);
        }
      }
    } else if (pkt->stream_index == demuxer->audio_stream_index()) {
    }
  }

  video_decoder->SendEOF();
  while (video_decoder->ReceiveFrame(video_frame) != AVERROR_EOF) {
    LOG(INFO) << "flush video "
      << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
      << " frame type:" << av_get_picture_type_char(video_frame->pict_type);
    CHECK(video_encoder->SendFrame(video_frame) == 0);
    if (video_encoder->ReceivePacket(h264_pkt) == 0) {
      LOG(INFO) << "encode h264."
        << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
        << " size:" << h264_pkt->size;
      muxer->WriteVideoPacket(h264_pkt, demuxer->video_stream()->time_base);
    }
  }

  video_encoder->SendEOF();
  while (video_encoder->ReceivePacket(h264_pkt) != AVERROR_EOF) {
    LOG(INFO) << "flush h264 pkt."
      << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
      << " size:" << h264_pkt->size;
    muxer->WriteVideoPacket(h264_pkt, demuxer->video_stream()->time_base);
  }

  LOG(INFO) << "finish.";
}