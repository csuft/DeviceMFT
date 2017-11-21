
extern "C" {
#include <libavutil/pixdesc.h>
}
#include <av_toolbox/video_parser.h>
#include "av_toolbox/ffmpeg_util.h"
#include <llog/llog.h>
#include <gtest/gtest.h>
using namespace ins;

TEST(video, parser) {
  InitFFmpeg();
  ins::VideoParser video_parser("http://192.168.2.123:8080/static/video/game-test.mp4");
  ASSERT_TRUE(video_parser.Open());
  sp<AVFrame> img;
  ASSERT_EQ(video_parser.ScreenshotAt(20 * 1000, img), 0);
  LOG(INFO) << " pts:" << img->pts << " width:" << img->width << " height:" << img->height
    << " fmt:" << av_get_pix_fmt_name(static_cast<AVPixelFormat>(img->format));
}