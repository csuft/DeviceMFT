#include "filter_util.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <llog/llog.h>

namespace ins {

sp<AVBSFContext> NewAVBSFContext(const std::string& filter_name, const AVStream *st) {
  AVBSFContext *ctx = nullptr;
  auto filter = av_bsf_get_by_name(filter_name.c_str());
  auto ret = av_bsf_alloc(filter, &ctx);
  if (ret != 0) return nullptr;
  CHECK(avcodec_parameters_copy(ctx->par_in, st->codecpar) >= 0);
  ctx->time_base_in = st->time_base;

  sp<AVBSFContext> sp_ctx(ctx, [](AVBSFContext *ctx) {
    av_bsf_free(&ctx);
  });
  return sp_ctx;
}

}
