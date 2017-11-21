//
// Created by jerett on 16/12/2.
//

#include "ffmpeg_util.h"
#include <llog/llog.h>

namespace ins {

static void FFmpegLogCallback(void* ptr, int level, const char* fmt, va_list vl) {
  if (level <= AV_LOG_ERROR) {
    char buf[1024];
    vsprintf(buf, fmt, vl);
    LOG(ERROR) << "FFmpeg Log:"<<  buf;
  }
 /*   vfprintf(stdout, fmt, vl);*/
}

void InitFFmpeg() {
  static std::once_flag init_once;
  std::call_once(init_once, []() {
    LOG(INFO) << "ffmpeg init";
    av_register_all();
    avformat_network_init();
    av_log_set_callback(FFmpegLogCallback);
  });
}

std::string FFmpegErrorToString(int code) {
  char str[AV_ERROR_MAX_STRING_SIZE] = {0};
  return std::string(av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, code));
}

}


