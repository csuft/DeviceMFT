//
// Created by jerett on 16/7/13.
//

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include <gtest/gtest.h>
#include <condition_variable>
#include <editor/editor.h>
#include <av_toolbox/ffmpeg_util.h>

using namespace ins;
MediaPipe::MediaPipeState RunPipe(const std::shared_ptr<ins::MediaPipe> &pipe, int cancel_delay_sec = -1) {
  auto over = false;
  std::thread progress_thread([pipe, cancel_delay_sec, &over]() {
    ins::Timer cancel_timer;
    while (!over) {
      LOG(INFO) << "progress " << pipe->progress();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      if (cancel_delay_sec > 0 && cancel_timer.Pass() >= cancel_delay_sec * 1000) {
        pipe->Cancel();
      }
    }
  });

  std::mutex mtx;
  std::unique_lock<std::mutex> lck(mtx);
  std::condition_variable cond;
  MediaPipe::MediaPipeState ret_state;
  pipe->RegisterEndCallback([&]() {
    ret_state = MediaPipe::kMediaPipeEnd;
    LOG(INFO) << "pipe run over.";
    over = true;
    cond.notify_all();
  });

  pipe->RegisterCancelCallback([&]() {
    ret_state = MediaPipe::kMediaPipeCanceled;
    LOG(INFO) << "pipe run canceled.";
    over = true;
    cond.notify_all();
  });

  pipe->RegisterErrorCallback([&](int error_code) {
    ret_state = MediaPipe::kMediaPipeError;
    LOG(INFO) << "pipe run error, code:" << error_code;
    over = true;
    cond.notify_all();
  });

  pipe->Run();
  cond.wait(lck);
  progress_thread.join();
  pipe->Release();
  return ret_state;
}

TEST(editor, mux) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();

  std::map<std::string, std::string> options;
  //  sinker->SetOpt(ins::kMovFlag, ins::kMovFlag_FragmentMp4);
  auto src = std::make_shared<ins::FileMediaSrc>("/Users/jerett/Desktop/pano_sn3d_insta.mp4");
  //  src->SetOpt(ins::kSphericalProjection, std::string("equirectangular"));
  //src->SetOpt(ins::kSrcMode, std::string("re"));
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/mux_spherical.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/editor-trim_mux.mp4");
  //auto sinker = std::make_shared<ins::MuxSink>("flv", "rtmp://192.168.2.123/live/fuck");
#endif

  sinker->set_spherical_projection("equirectangular")
        ->set_stereo_type("mono")
        ->set_fragment_mp4(true, 100);
  sinker->set_spatial_audio(true);
  //  auto sinker = std::make_shared<ins::MuxSink>("flv", "rtmp://192.168.2.123/live/fuck");
  //  sinker->SetOpt(ins::kMovFlag, ins::kMovFlag_FragmentMp4);
  //  sinker->SetOpt(ins::kFragmentFrameCount, 100);

  {
    src->set_video_filter(sinker);
    src->set_audio_filter(sinker);
    src->set_trim_range(1 * 1000, 6 * 1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(editor, nullsink) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();

  std::map<std::string, std::string> options;
  //  sinker->SetOpt(ins::kMovFlag, ins::kMovFlag_FragmentMp4);
  auto src = std::make_shared<ins::FileMediaSrc>("/Users/jerett/Desktop/origin_0.mp4");
  //  src->SetOpt(ins::kSphericalProjection, std::string("equirectangular"));
  //src->SetOpt(ins::kSrcMode, std::string("re"));
  auto null_sink = std::make_shared<ins::NullSink<sp<AVFrame>>>();

  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_enable_hwaccel(true);
    src->set_video_filter(video_decode_filter)
        ->set_next_filter(null_sink);
    src->set_trim_range(0 * 1000, 1 * 1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(editor, cpu_transcode) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();
  std::map<std::string, std::string> options;
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/cpu_transcode.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/editor_trim_transcode.mp4");
#endif
//  auto null_sinker = std::make_shared<NullSink<sp<ARVPacket>>>();
  auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/41.mp4");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/8K.mp4");
  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_threads(4)
                       ->set_enable_hwaccel(false);

    auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto video_encode_filter = std::make_shared<ins::EncodeFilter>(AV_CODEC_ID_H264);
    auto statistic_filter = std::make_shared<StatisticFilter<typename decltype(sinker)::element_type::in_type>>();

    video_encode_filter->set_enable_hwaccel(false)
                       ->set_threads(4)
                       ->set_bitrate(50 * 1024 * 1024);
    src->set_video_filter(video_decode_filter)
       ->set_next_filter(queue_filter)
       ->set_next_filter(video_encode_filter)
       ->set_next_filter(statistic_filter)
       ->set_next_filter(sinker);

    src->set_audio_filter(sinker);
    //src->set_trim_range(10*1000, 15*1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(editor, hwaccel_transcode) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();
  std::map<std::string, std::string> options;
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/hwaccel_transcode_h264.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/hwaccel_transcode_h264.mp4");
#endif

  auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/contrast/origin_5.mp4");
//  auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/120fps/origin_5.mp4");
  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_threads(4)
                       ->set_copy_hwaccel(false)
                       ->set_enable_hwaccel(true);

    auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto video_encode_filter = std::make_shared<ins::EncodeFilter>(AV_CODEC_ID_H264);
    video_encode_filter->set_enable_hwaccel(true)
                       ->set_bitrate(50 * 1024 * 1024);
    auto statistic_filter = std::make_shared<StatisticFilter<typename decltype(sinker)::element_type::in_type>>();

    src->set_video_filter(video_decode_filter)
       ->set_next_filter(queue_filter)
       ->set_next_filter(video_encode_filter)
       ->set_next_filter(statistic_filter)
       ->set_next_filter(sinker);

//    auto audio_decode_filter = std::make_shared<DecodeFilter>();
//    audio_decode_filter->set_enable_hwaccel(false);
//    auto audio_encode_filter = std::make_shared<EncodeFilter>(AV_CODEC_ID_AAC);
//    audio_encode_filter->set_enable_hwaccel(false);
//    src->set_audio_filter(audio_decode_filter)
//      ->set_next_filter(audio_encode_filter)
//      ->set_next_filter(sinker);
    src->set_audio_filter(sinker);
    //src->set_trim_range(10*1000, 15*1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(editor, cpu_transcode_h265) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();
  std::map<std::string, std::string> options;
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/editor_trim_transcode.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/cpu_transcode_h265.mp4");
#endif

  auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/contrast/origin_5.mp4");
  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_threads(4)
                       ->set_copy_hwaccel(true)
                       ->set_enable_hwaccel(true);

    auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto video_encode_filter = std::make_shared<ins::EncodeFilter>(AV_CODEC_ID_H265);
    auto scale_filter = std::make_shared<ins::ScaleFilter>(1920, 960, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR);
    auto statistic_filter = std::make_shared<StatisticFilter<typename decltype(sinker)::element_type::in_type>>();

    video_encode_filter->set_enable_hwaccel(false)
                       ->set_bitrate(20 * 1024 * 1024);
    src->set_video_filter(video_decode_filter)
      ->set_next_filter(scale_filter)
      ->set_next_filter(queue_filter)
      ->set_next_filter(video_encode_filter)
      ->set_next_filter(statistic_filter)
      ->set_next_filter(sinker);

    src->set_audio_filter(sinker);
    src->set_trim_range(10*1000, 15*1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(editor, hwaccel_transcode_h265) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();
  std::map<std::string, std::string> options;
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/hwaccel_transcode_h265.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/hwaccel_transcode_h265.mp4");
#endif

  auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/contrast/origin_5.mp4");
  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_threads(4)
                       ->set_copy_hwaccel(false)
                       ->set_enable_hwaccel(true);

    //auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto video_encode_filter = std::make_shared<ins::EncodeFilter>(AV_CODEC_ID_H265);
    //auto scale_filter = std::make_shared<ins::ScaleFilter>(3840, 1920, AV_PIX_FMT_NV12, SWS_FAST_BILINEAR);
    //auto scale_queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto statistic_filter = std::make_shared<StatisticFilter<typename decltype(sinker)::element_type::in_type>>();

    video_encode_filter->set_enable_hwaccel(true)
                       ->set_bitrate(20 * 1024 * 1024);
    src->set_video_filter(video_decode_filter)
      //->set_next_filter(scale_queue_filter)
      //->set_next_filter(scale_filter)
      //->set_next_filter(queue_filter)
      ->set_next_filter(video_encode_filter)
      ->set_next_filter(statistic_filter)
      ->set_next_filter(sinker);

    src->set_audio_filter(sinker);
    //src->set_trim_range(10*1000, 15*1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(editor, hwaccel_decode_cpu_encode) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();
  std::map<std::string, std::string> options;
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/editor_trim_transcode.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/editor_trim_transcode_h265.mp4");
#endif

  //auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/41.mp4");
  auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/contrast/origin_5.mp4");
  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_threads(4)
      ->set_copy_hwaccel(false)
      ->set_enable_hwaccel(false);

    auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto video_encode_filter = std::make_shared<ins::EncodeFilter>(AV_CODEC_ID_H264);
    video_encode_filter->set_enable_hwaccel(true);
    auto statistic_filter = std::make_shared<StatisticFilter<typename decltype(sinker)::element_type::in_type>>();

    video_encode_filter->set_enable_hwaccel(true)
      ->set_bitrate(80 * 1024 * 1024);
    src->set_video_filter(video_decode_filter)
      ->set_next_filter(queue_filter)
      ->set_next_filter(video_encode_filter)
      ->set_next_filter(statistic_filter)
      ->set_next_filter(sinker);

    src->set_audio_filter(sinker);
    //src->set_trim_range(10*1000, 15*1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}


TEST(editor, hwaccel_transcode_failed) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();
  std::map<std::string, std::string> options;
#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/editor_trim_transcode.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/editor_trim_transcode.mp4");
#endif

  //auto src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
  //auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/pro/41.mp4");
  auto src = std::make_shared<ins::FileMediaSrc>("D:/Videos/8K.mp4");
  {
    auto video_decode_filter = std::make_shared<ins::DecodeFilter>();
    video_decode_filter->set_threads(4)
      ->set_enable_hwaccel(true);

    auto queue_filter = std::make_shared<ins::QueueFilter<ins::sp<AVFrame>>>();
    auto video_encode_filter = std::make_shared<ins::EncodeFilter>(AV_CODEC_ID_H264);
    video_encode_filter->set_enable_hwaccel(true)
                       ->set_bitrate(50 * 1024 * 1024);
    src->set_video_filter(video_decode_filter)
       ->set_next_filter(queue_filter)
       ->set_next_filter(video_encode_filter)
       ->set_next_filter(sinker);

    src->set_audio_filter(sinker);
    //src->set_trim_range(10*1000, 15*1000);
  }
  pipe->AddMediaSrc(src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeError);
}


TEST(editor, merge_audio) {
  ins::InitFFmpeg();
  auto pipe = std::make_shared<ins::MediaPipe>();

  std::map<std::string, std::string> options;
  //auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/fuck.mp4");

#if __APPLE__
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "/Users/jerett/Desktop/editor-merge_audio.mp4");
#elif _WIN32
  auto sinker = std::make_shared<ins::MuxSink>("mp4", "C:/Users/wjjia/Desktop/editor-merge_audio.mp4");
#endif
  {
    auto video_src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/shenda.insv");
    video_src->set_video_filter(sinker);
    //      video_src->set_trim_range(10 * 1000, 15 * 1000);
    pipe->AddMediaSrc(video_src);
  } {
    auto audio_src = std::make_shared<ins::FileMediaSrc>("http://192.168.2.123:8080/static/video/audio.mp4");
    //      audio_src->set_video_filter(sinker);
    //      video_src->set_trim_range(10 * 1000, 15 * 1000);
    audio_src->set_trim_range(10 * 1000, 20 * 1000);
    audio_src->set_audio_filter(sinker);
    pipe->AddMediaSrc(audio_src);
  }
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}
