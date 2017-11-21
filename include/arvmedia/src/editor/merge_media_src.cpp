#include "merge_media_src.h"
#include <llog/llog.h>

namespace ins {

double MergeMediaSrc::progress() const {
  return (left_src_->progress() + right_src_->progress()) / 2.0;
}

bool MergeMediaSrc::Prepare() {
  left_src_->RegisterCallback([this](MediaSrcState state, const any &extra) {
    if (state == MediaSourceEnd) {
      std::lock_guard<std::mutex> lck(cur_src_mtx_);
      cur_src_ = right_src_;
      int64_t offset_ms = any_cast<int64_t>(extra);
      LOG(INFO) << "start right src:" << offset_ms;
      cur_src_->Start(offset_ms);
    } else {
      state_callback_(state, extra);
    }
  });
  right_src_->RegisterCallback([this](MediaSrcState state, const any &extra) {
    state_callback_(state, extra);
  });
  return left_src_->Prepare() && right_src_->Prepare();
}

void MergeMediaSrc::Start(int64_t timestamp_offset_ms) {
  std::lock_guard<std::mutex> lck(cur_src_mtx_);
  cur_src_->Start(timestamp_offset_ms);
}

void MergeMediaSrc::Cancel() {
  std::lock_guard<std::mutex> lck(cur_src_mtx_);
  cur_src_->Cancel();
}

void MergeMediaSrc::Close() {
  std::lock_guard<std::mutex> lck(cur_src_mtx_);
  left_src_->Close();
  if (cur_src_ == right_src_) {
    right_src_->Close();
  }
}

}
