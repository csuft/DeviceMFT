
//
// Created by jerett on 2017/6/14.
//

#include "swr_util.h"
extern "C" {
#include <libswresample/swresample.h>
}

namespace ins {

sp<SwrContext> NewSwrContext(int64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate,
                             int64_t  in_ch_layout, AVSampleFormat  in_sample_fmt, int  in_sample_rate,
                             int log_offset, void *log_ctx) {
  auto ctx = swr_alloc_set_opts(nullptr, out_ch_layout, out_sample_fmt, out_sample_rate,
                                in_ch_layout, in_sample_fmt, in_sample_rate,
                                log_offset, log_ctx);
  sp<SwrContext> sp_ctx(ctx, [](SwrContext *ctx) {
    swr_free(&ctx);
  });
  return sp_ctx;
}

}