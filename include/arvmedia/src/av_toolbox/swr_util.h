
//
// Created by jerett on 2017/6/14.
//

#pragma once

extern "C" {
#include <libavutil/samplefmt.h>
}
#include "sp.h"

struct SwrContext;

namespace ins {

sp<SwrContext> NewSwrContext(int64_t out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate,
                             int64_t  in_ch_layout, AVSampleFormat  in_sample_fmt, int  in_sample_rate,
                             int log_offset, void *log_ctx);
}
