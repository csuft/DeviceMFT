//
// Created by jerett on 17/1/12
//

#include <gtest/gtest.h>
extern "C" {
#include <libavutil/pixdesc.h>
}
#include <editor/editor.h>
#include <av_toolbox/demuxer.h>
#include <av_toolbox/ffmpeg_util.h>

TEST(realtime_src, mux) {
  using namespace std::chrono;
  using namespace ins;
  InitFFmpeg();

  sp<Demuxer> demuxer;
  {
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/game-test.mp4"));
    //demuxer.reset(new Demuxer("/Users/jerett/Desktop/shenda.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
  }
  auto video_stream = demuxer->video_stream();
  CHECK(video_stream != nullptr);

  auto raw_src = std::make_shared<RealtimeMediaSrc<sp<ARVPacket>, sp<ARVPacket>>>();
//#if __APPLE__
//  
//#elif _WIN32
//  //auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/rawmedia-fuck.mp4");
//  //auto sinker = std::make_shared<ins::MuxSink>("flv", "rtmp://192.168.2.123/live/fuck");
//#endif

#if ON_WIN64
  //auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/rawmedia-fuck.mp4");
  //auto sinker = std::make_shared<ins::MuxSink>("flv", "rtmp://192.168.2.123/live/fuck");
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/realtime_src-mux.mp4");
#elif ON_APPLE
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/realtime_src_mux.mp4");
#endif


#if __APPLE__
    printf("fewfw\n");
#endif

  {
    //mux only 
    auto vst = NewH264Stream(video_stream->codecpar->width, video_stream->codecpar->height,
                             video_stream->avg_frame_rate, video_stream->time_base,
                             AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P,
                             video_stream->codecpar->extradata, video_stream->codecpar->extradata_size);
    raw_src->AddVideoStream(vst);
    raw_src->set_video_filter(sinker);
  }

  {
    auto audio_stream = demuxer->audio_stream();
    CHECK(audio_stream != nullptr);
    auto ast = NewAACStream(static_cast<AVSampleFormat>(audio_stream->codecpar->format),
                            audio_stream->codecpar->channel_layout,
                            audio_stream->codecpar->channels,
                            audio_stream->codecpar->sample_rate,
                            audio_stream->codecpar->extradata, audio_stream->codecpar->extradata_size,
                            audio_stream->time_base);
    raw_src->AddAudioStream(ast);
    raw_src->set_audio_filter(sinker);
  }

  {
    CHECK(raw_src->Prepare() && sinker->Open());
  }

  sp<ARVPacket> pkt;
  sp<AVFrame> video_frame;

  Timer timer;
  while (demuxer->NextPacket(pkt) == 0) {
    auto timestamp = TimestampToMs(pkt->dts, demuxer->StreamAtIndex(pkt->stream_index)->time_base);
    auto pass_ms = timer.Pass();
    Ignore(timestamp);
    Ignore(pass_ms);

    //    if (timestamp > pass_ms) {
    //      auto sleep_time = timestamp - pass_ms;
    //      std::this_thread::sleep_for(milliseconds(sleep_time));
    ////      auto real_sleep_time = timer.Pass() - pass_ms;
    //    } else {
    //      //LOG(INFO) << "delay " << (pass_ms - timestamp) << " ms"
    //      //  << " timestamp:" << timestamp << " pass:" << pass_ms;
    //    }

    if (pkt->stream_index == demuxer->video_stream_index()) {
      auto h264_pkt = NewH264Packet(pkt->data, pkt->size, pkt->pts, pkt->dts, IsKeyFrame(pkt), 0);
      //raw_src->SendVideo(pkt);
      raw_src->SendVideo(h264_pkt);
    } else if (pkt->stream_index == demuxer->audio_stream_index()) {
      /*   LOG(INFO) << "audio pts:" << pkt->pts << " dts:" << pkt->dts << " duration:" << pkt->duration
      << " flags:" << pkt->flags << "  size:" << pkt->size << " pos:" << pkt->pos;*/
      auto aac_pkt = NewAACPacket(pkt->data, pkt->size, pkt->pts, 1);
      //LOG(INFO) << "aac audio pts:" << aac_pkt->pts << " dts:" << aac_pkt->dts << " duration:" << aac_pkt->duration
      //  << " flags:" << aac_pkt->flags << " size:" << aac_pkt->size << " pos:" << aac_pkt->pos;
      raw_src->SendAudio(aac_pkt);
      //raw_src->SendAudio(pkt);
    }
  }
  raw_src->Stop();
  LOG(INFO) << "finish.";
}

TEST(realtime_src, transcode) {
  using namespace std::chrono;
  using namespace ins;
  InitFFmpeg();

  sp<Demuxer> demuxer;
  {
    demuxer.reset(new Demuxer("http://192.168.2.123:8080/static/video/game-test.mp4"));
    //demuxer.reset(new Demuxer("/Users/jerett/Desktop/shenda.insv"));
    CHECK(demuxer->Open(nullptr) == 0);
  }
  auto video_stream = demuxer->video_stream();
  CHECK(video_stream != nullptr);

  sp<Decoder> video_decoder;
  {
    auto dec_ctx = NewAVHwaccelDecodeContextFromID(video_stream->codecpar->codec_id, true);
    if (dec_ctx == nullptr) {
      dec_ctx = NewAVDecodeContextFromID(video_stream->codecpar->codec_id);
    }
    CHECK(dec_ctx != nullptr);
    dec_ctx->refcounted_frames = 1;
    auto ret = avcodec_parameters_to_context(dec_ctx.get(), video_stream->codecpar);
    CHECK(ret == 0);
    video_decoder.reset(new Decoder(dec_ctx));
    CHECK(video_decoder->Open() == 0);
  }

  //auto raw_src = std::make_shared<RawMediaSrc<sp<AVFrame>, sp<AVPacket>>>();
  auto raw_src = std::make_shared<RealtimeMediaSrc<sp<AVFrame>, sp<ARVPacket>>>();
  //auto raw_src = std::make_shared<RawMediaSrc<std::nullptr_t, sp<AVPacket>>>();

#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/realtime_src_transcode.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/realtime_src-transcode.mp4");
  //auto sinker = std::make_shared<ins::MuxSink>("flv", "rtmp://192.168.2.123/live/fuck");
  //auto sinker = std::make_shared<ins::MuxSink>("flv", "C:/Users/wjjia/Desktop/rawmedia-fuck.flv");
#endif

  {
    //transcode video
    //auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    //use circular buffer
    auto circular_buffer_filter = std::make_shared<CircularBufferFilter<sp<AVFrame>>>(10);

    auto video_encode_filter = std::make_shared<EncodeFilter>(AV_CODEC_ID_H264);
    //video_encode_filter->SetOpt(ins::kUseHwaccelEncode, false);
    //video_encode_filter->SetOpt(ins::kX264EncodePreset, std::string("fast"));
    //video_encode_filter->SetOpt(ins::kX264EncodeTune, std::string("zerolatency"));
    video_encode_filter->set_bitrate(5 * 1024 * 1024);

    auto vst = NewVideoStream(video_stream->codecpar->width, video_stream->codecpar->height,
                              video_stream->avg_frame_rate, video_stream->time_base,
                              video_decoder->dec_ctx()->codec_id, video_decoder->dec_ctx()->pix_fmt);
    raw_src->AddVideoStream(vst);
    raw_src->set_video_filter(circular_buffer_filter)
      ->set_next_filter(video_encode_filter)
      ->set_next_filter(sinker);
  }

  {
    auto audio_stream = demuxer->audio_stream();
    CHECK(audio_stream != nullptr);
    auto ast = NewAACStream(static_cast<AVSampleFormat>(audio_stream->codecpar->format),
                            audio_stream->codecpar->channel_layout,
                            audio_stream->codecpar->channels,
                            audio_stream->codecpar->sample_rate,
                            audio_stream->codecpar->extradata, audio_stream->codecpar->extradata_size,
                            audio_stream->time_base);
    raw_src->AddAudioStream(ast);
    raw_src->set_audio_filter(sinker);
  }

  {
    CHECK(raw_src->Prepare() && sinker->Open());
  }

  sp<ARVPacket> pkt;
  sp<AVFrame> video_frame;

  Timer timer;
  while (demuxer->NextPacket(pkt) == 0) {
    auto timestamp = TimestampToMs(pkt->pts, demuxer->StreamAtIndex(pkt->stream_index)->time_base);
    auto pass_ms = timer.Pass();
    if (timestamp > pass_ms) {
      //std::this_thread::sleep_for(milliseconds(timestamp - pass_ms));
    } else {
      //LOG(INFO) << "delayed.";
    }

    if (pkt->stream_index == demuxer->video_stream_index()) {
      CHECK(video_decoder->SendPacket(pkt) == 0);
      if (video_decoder->ReceiveFrame(video_frame) == 0) {
        raw_src->SendVideo(video_frame);
        //LOG(INFO) << "video decode:" << TimestampToMs(pkt->pts, demuxer->video_stream()->time_base)
        //  << " type:" << av_pix_fmt_desc_get(static_cast<AVPixelFormat>(video_frame->format))->name;
      }
    } else if (pkt->stream_index == demuxer->audio_stream_index()) {
      raw_src->SendAudio(pkt);
    }
  }

  video_decoder->SendEOF();
  while (video_decoder->ReceiveFrame(video_frame) != AVERROR_EOF) {
    LOG(INFO) << "flush video "
      << " ms:" << TimestampToMs(video_frame->pts, demuxer->video_stream()->time_base)
      << " frame type:" << av_get_picture_type_char(video_frame->pict_type);
    raw_src->SendVideo(video_frame);
  }
  raw_src->Stop();
  LOG(INFO) << "finish.";
}
