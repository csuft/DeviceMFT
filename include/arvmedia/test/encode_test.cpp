#include <gtest/gtest.h>
#include <winerror.h>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
}
#include <av_toolbox/demuxer.h>
#include <av_toolbox/decoder.h>
#include <av_toolbox/scaler.h>
#include <av_toolbox/ffmpeg_util.h>
#include <av_toolbox/encoder.h>
#include <llog/llog.h>

using namespace ins;

TEST(encode, video_cpu) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  {
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/shenda.insv"));
    //    demuxer.reset(new Demuxer("/Users/jerett/Desktop/a.mp4"));
    ASSERT_EQ(demuxer->Open(nullptr), 0);
    auto video_duration = TimestampToMs(demuxer->video_stream()->duration, demuxer->video_stream()->time_base);
  }

  auto vst = demuxer->video_stream();
  auto dec_ctx = NewAVDecodeContextFromID(vst->codecpar->codec_id);
  avcodec_parameters_to_context(dec_ctx.get(), vst->codecpar);
  Decoder video_decoder(dec_ctx);
  ASSERT_EQ(video_decoder.Open(), 0);

  auto codec_par = NewAVCodecParameters();
  avcodec_parameters_from_context(codec_par.get(), dec_ctx.get());
  auto enc_ctx = NewAVEncodeContextFromID(vst->codecpar->codec_id);
  enc_ctx->time_base = vst->time_base;
  avcodec_parameters_to_context(enc_ctx.get(), codec_par.get());
  Encoder video_encoder(enc_ctx);
  ASSERT_EQ(video_encoder.Open(), 0);

  LOG(INFO) << "encode extradata size:" << video_encoder.enc_ctx()->extradata_size;
  //  for (int i = 0; i < video_encoder->enc_ctx()->extradata_size; ++i) {
  //    printf("%0x ", video_encoder->enc_ctx()->extradata[i]);
  //  }
  //  printf("\n");

  sp<AVFrame> video_frame;
  sp<AVFrame> audio_frame;
  sp<ARVPacket> h264_pkt;
  sp<ARVPacket> pkt;
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      ASSERT_EQ(video_decoder.SendPacket(pkt), 0);
      if (video_decoder.ReceiveFrame(video_frame) == 0) {
        //LOG(INFO) << "decode video,"
        //  << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
        //  << " frame type:" << av_get_picture_type_char(video_frame->pict_type);
        //video_frame->pict_type = AV_PICTURE_TYPE_I;
        ASSERT_EQ(video_encoder.SendFrame(video_frame), 0);
        if (video_encoder.ReceivePacket(h264_pkt) == 0) {
          //LOG(INFO) << "encode h264."
          //  << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
          //  << " size:" << h264_pkt->size;
          LOG(INFO) << "h264 pkt is key:" << IsKeyFrame(h264_pkt);
        }
      }
    }
  }

  video_decoder.SendEOF();
  while (video_decoder.ReceiveFrame(video_frame) != AVERROR_EOF) {
    LOG(INFO) << "flush video "
      << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
      << " frame type:" << av_get_picture_type_char(video_frame->pict_type);
    ASSERT_EQ(video_encoder.SendFrame(video_frame), 0);
    if (video_encoder.ReceivePacket(h264_pkt) == 0) {
      LOG(INFO) << "encode h264."
        << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
        << " size:" << h264_pkt->size;
    }
  }

  video_encoder.SendEOF();
  while (video_encoder.ReceivePacket(h264_pkt) != AVERROR_EOF) {
    LOG(INFO) << "flush h264 pkt."
      << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
      << " size:" << h264_pkt->size;
  }

  LOG(INFO) << "finish.";
}


TEST(encode, video_hwaccel) {
  InitFFmpeg();
  sp<Demuxer> demuxer;
  {
    //demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/shenda.insv"));
    demuxer.reset(new Demuxer("D:/videos/pro/41.mp4"));
    //    demuxer.reset(new Demuxer("/Users/jerett/Desktop/a.mp4"));
    ASSERT_EQ(demuxer->Open(nullptr), 0);
  }

  auto vst = demuxer->video_stream();

  sp<Decoder> video_decoder;
  auto dec_ctx = NewAVHwaccelDecodeContextFromID(vst->codecpar->codec_id);
  dec_ctx->refcounted_frames = 1;
  avcodec_parameters_to_context(dec_ctx.get(), vst->codecpar);
  video_decoder.reset(new Decoder(dec_ctx));
  ASSERT_EQ(video_decoder->Open(), 0);

  auto codec_par = NewAVCodecParameters();
  avcodec_parameters_from_context(codec_par.get(), dec_ctx.get());

  auto enc_ctx = NewAVHwaccelEncodeContextFromID(vst->codecpar->codec_id);
  avcodec_parameters_to_context(enc_ctx.get(), codec_par.get());
  auto device_ref = reinterpret_cast<AVHWFramesContext*>(dec_ctx->hw_frames_ctx->data)->device_ref;
  ASSERT_NE(device_ref, nullptr);
  auto hwframe_ctx_ref = av_hwframe_ctx_alloc(device_ref);
  ASSERT_NE(hwframe_ctx_ref, nullptr);
  auto hwframe_ctx = reinterpret_cast<AVHWFramesContext*>(hwframe_ctx_ref->data);
  hwframe_ctx->format = AV_PIX_FMT_CUDA;
  hwframe_ctx->sw_format = AV_PIX_FMT_NV12;
  hwframe_ctx->width = enc_ctx->width;
  hwframe_ctx->height = enc_ctx->height;
  auto ret = av_hwframe_ctx_init(hwframe_ctx_ref);
  ASSERT_EQ(ret, 0);
  enc_ctx->hw_frames_ctx = hwframe_ctx_ref;
  enc_ctx->pix_fmt = AV_PIX_FMT_CUDA;
  enc_ctx->sw_pix_fmt = AV_PIX_FMT_NV12;

  //enc_ctx->hw_frames_ctx = av_buffer_ref(dec_ctx->hw_frames_ctx);
  //auto device_buffer = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);

  //ASSERT_EQ(av_hwframe_ctx_init(hwframe_buffer), 0);
  enc_ctx->time_base = vst->time_base;
  enc_ctx->pix_fmt = AV_PIX_FMT_CUDA;
  enc_ctx->sw_pix_fmt = AV_PIX_FMT_NV12;
  Encoder video_encoder(enc_ctx);
  ASSERT_EQ(video_encoder.Open(), 0);

  LOG(INFO) << "encode extradata size:" << video_encoder.enc_ctx()->extradata_size;

  sp<AVFrame> video_frame;
  sp<AVFrame> audio_frame;
  sp<ARVPacket> h264_pkt;
  sp<ARVPacket> pkt;
  ins::Timer fps_timer;
  auto frame_cnt = 0;

  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->video_stream_index()) {
      ASSERT_EQ(video_decoder->SendPacket(pkt), 0);
      if (video_decoder->ReceiveFrame(video_frame) == 0) {
        //LOG(INFO) << "decode video,"
        //  << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
        //  << " fmt:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(video_frame->format))->name
        //  << " width:" << video_frame->width << " height:" << video_frame->height
        //  << " linesize[0]:" << video_frame->linesize[0]
        //  << " linesize[1]:" << video_frame->linesize[1];

        ASSERT_EQ(video_encoder.SendFrame(video_frame), 0);
        if (video_encoder.ReceivePacket(h264_pkt) == 0) {
          //LOG(INFO) << "h264 pkt is key:" << IsKeyFrame(h264_pkt);
          //LOG(INFO) << "encode h264."
          //  << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
          //  << " size:" << h264_pkt->size;
        }
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

  video_decoder->SendEOF();
  while (video_decoder->ReceiveFrame(video_frame) != AVERROR_EOF) {
    LOG(INFO) << "flush video "
      << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
      << " frame type:" << av_get_picture_type_char(video_frame->pict_type);
    ASSERT_EQ(video_encoder.SendFrame(video_frame), 0);
    if (video_encoder.ReceivePacket(h264_pkt) == 0) {
      LOG(INFO) << "encode h264."
        << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
        << " size:" << h264_pkt->size;
    }
  }

  video_encoder.SendEOF();
  while (video_encoder.ReceivePacket(h264_pkt) != AVERROR_EOF) {
    LOG(INFO) << "flush h264 pkt."
      << " ms:" << TimestampToMs(h264_pkt->pts, demuxer->video_stream()->time_base)
      << " size:" << h264_pkt->size;
  }

  LOG(INFO) << "finish.";
}
