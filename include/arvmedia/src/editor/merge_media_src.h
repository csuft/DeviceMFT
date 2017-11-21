#pragma once

#include "media_src.h"
#include "av_toolbox/sp.h"
#include <mutex>

namespace ins {

class MergeMediaSrc : public MediaSrc {
public:
  MergeMediaSrc(const sp<MediaSrc> &left_src, const sp<MediaSrc> &right_src) :
    left_src_(left_src), right_src_(right_src), cur_src_(left_src)
  {
  }

  MergeMediaSrc(const MergeMediaSrc&) = delete;
  MergeMediaSrc(MergeMediaSrc&&) = delete;
  MergeMediaSrc& operator=(const MergeMediaSrc&) = delete;

private:
  double progress() const override;
  bool Prepare() override;
  void Start(int64_t timestamp_offset_ms = 0) override;
  void Cancel() override;
  void Close() override;

private:
  sp<MediaSrc> left_src_;
  sp<MediaSrc> right_src_;
  std::mutex cur_src_mtx_;
  sp<MediaSrc> cur_src_;
};

}
