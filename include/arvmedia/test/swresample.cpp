
#include <gtest/gtest.h>
extern "C" {
#include <libswresample/swresample.h>
}
#include <llog/llog.h>
#include <av_toolbox/demuxer.h>
#include <av_toolbox/decoder.h>
#include <av_toolbox/encoder.h>
#include <av_toolbox/muxer.h>
#include <av_toolbox/scaler.h>
#include <av_toolbox/video_parser.h>
#include <av_toolbox/bsf_filter.h>
#include <av_toolbox/ffmpeg_util.h>

using namespace ins;

TEST(av, audio_swreample) {
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
  auto audio_stream = demuxer->audio_stream();

  auto ast = demuxer->audio_stream();
  auto dec_ctx = NewAVDecodeContextFromID(ast->codecpar->codec_id);
  avcodec_parameters_to_context(dec_ctx.get(), ast->codecpar);
  Decoder audio_decoder(dec_ctx);
  CHECK(audio_decoder.Open() == 0);

  sp<ARVPacket> pkt;
  sp<AVFrame> audio_frame;

  AVSampleFormat out_fmt;
  uint64_t out_layout;
  int out_sample_rate;

  SwrContext *swr_ctx = nullptr;
  while (demuxer->NextPacket(pkt) == 0) {
    if (pkt->stream_index == demuxer->audio_stream_index()) {
      CHECK(audio_decoder.SendPacket(pkt) == 0);
      if (audio_decoder.ReceiveFrame(audio_frame) == 0) {
        if (!swr_ctx) {
          auto src_sample_rate = audio_frame->sample_rate;
          auto src_layout = audio_frame->channel_layout;
          auto src_fmt = static_cast<AVSampleFormat>(audio_frame->format);
          auto src_channels = audio_stream->codecpar->channels;

          out_fmt = AV_SAMPLE_FMT_S16;
          out_layout = AV_CH_LAYOUT_STEREO;
          out_sample_rate = src_sample_rate;
          LOG(INFO) << " sample_fmt:" << av_get_sample_fmt_name(src_fmt)
            << " sample_rate:" << src_sample_rate
            << " channel:" << src_channels;
          swr_ctx = swr_alloc_set_opts(nullptr,
                                       out_layout, out_fmt, out_sample_rate,
                                       src_layout, src_fmt, src_sample_rate,
                                       0, nullptr);
          CHECK(swr_ctx != nullptr);
          LOG(VERBOSE) << "init result:" << swr_init(swr_ctx);
        }
        LOG(INFO) << "decode audio,"
          << " pts:" << audio_frame->pts
          << " fmt:" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(audio_frame->format))
          << " sample_rate:" << audio_frame->sample_rate
          << " channel:" << audio_frame->channels
          << " size:" << audio_frame->linesize[0]
          << " nb_samples:" << audio_frame->nb_samples;
        //        CHECK(audio_frame->format == src_fmt);
        //        CHECK(audio_frame->sample_rate == src_sample_rate);
        //        CHECK(audio_frame->channel_layout == src_layout);

        auto out_frame = NewAVFrame();
        out_frame->channel_layout = out_layout;
        out_frame->sample_rate = out_sample_rate;
        out_frame->format = out_fmt;
        av_samples_alloc(out_frame->data, out_frame->linesize, av_get_channel_layout_nb_channels(out_layout),
                         audio_frame->nb_samples, out_fmt, 0);
        //        out_frame->data[0] = new uint8_t[4096];
        //        auto ret = swr_convert(swr_ctx, out_frame->data, audio_frame->nb_samples,
        //                               (const uint8_t **)audio_frame->data, audio_frame->nb_samples);
        auto ret = swr_convert_frame(swr_ctx, out_frame.get(), audio_frame.get());
        CHECK(ret == 0) << FFmpegErrorToString(ret);
        av_frame_copy_props(out_frame.get(), audio_frame.get());

        LOG(INFO) << " pts:" << out_frame->pts
          << " fmt:" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(out_frame->format))
          << " channel:" << out_frame->channels
          << " size:" << out_frame->linesize[0];
      }
    }
  }
  swr_free(&swr_ctx);
  LOG(INFO) << "finish.";
}
