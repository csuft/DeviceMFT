extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}
#include <gtest/gtest.h>
#include <condition_variable>
#include <editor/editor.h>
#include <av_toolbox/ffmpeg_util.h>

using namespace ins;
namespace {
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
}

TEST(merge_src, mux) {
  InitFFmpeg();
  auto pipe = std::make_shared<MediaPipe>();

  auto sinker = std::make_shared<MuxSink>("mp4", "D:/Videos/TestOut/merge_src_mux.mp4");
  sp<MergeMediaSrc> merge_media_src;
  {
    auto src = std::make_shared<FileMediaSrc>("D:/Videos/shenda.mp4");
    src->set_video_filter(sinker);
    src->set_audio_filter(sinker);

    auto src2 = std::make_shared<FileMediaSrc>("D:/Videos/shenda2.mp4");
    src2->set_video_filter(sinker);
    src2->set_audio_filter(sinker);
    
    merge_media_src = std::make_shared<MergeMediaSrc>(src, src2);
  }

  pipe->AddMediaSrc(merge_media_src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}

TEST(merge_src, transcode) {
  InitFFmpeg();
  auto pipe = std::make_shared<MediaPipe>();

  auto decode_filter = std::make_shared<DecodeFilter>();
  decode_filter->set_enable_hwaccel(true)
    ->set_copy_hwaccel(true);
  auto scale_filter = std::make_shared<ScaleFilter>(1920, 960, AV_PIX_FMT_NV12, SWS_FAST_BILINEAR);
  auto encode_filter = std::make_shared<EncodeFilter>(AV_CODEC_ID_H264);
  auto sinker = std::make_shared<MuxSink>("mp4", "D:/Videos/TestOut/merge_src_transcode.mp4");
  sp<MergeMediaSrc> merge_media_src;
  {
    auto src = std::make_shared<FileMediaSrc>("D:/Videos/shenda.mp4");
    src->set_video_filter(decode_filter)
      ->set_next_filter(scale_filter)
      ->set_next_filter(encode_filter)
      ->set_next_filter(sinker);

    src->set_audio_filter(sinker);

    auto src2 = std::make_shared<FileMediaSrc>("D:/Videos/shenda2.mp4");
    src2->set_video_filter(decode_filter)
      ->set_next_filter(scale_filter)
      ->set_next_filter(encode_filter)
      ->set_next_filter(sinker);
    src2->set_audio_filter(sinker);
    
    merge_media_src = std::make_shared<MergeMediaSrc>(src, src2);
  }

  pipe->AddMediaSrc(merge_media_src);
  if (!pipe->Prepare()) {
    LOG(ERROR) << "pipe prepare error.";
    return;
  }
  ASSERT_EQ(RunPipe(pipe), MediaPipe::kMediaPipeEnd);
}
