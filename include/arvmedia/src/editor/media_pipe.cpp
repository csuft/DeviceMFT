#include "media_pipe.h"
#include <future>
#include <algorithm>
#include <eventloop/event_dispatcher.h>
#include <eventloop/event.h>
#include <av_toolbox/dynamic_bitset.h>
#include "media_src.h"

namespace ins {

enum {
  kToStart,
  kToCancel,
  kOnSrcEnd,
  kOnSrcCanceled,
  kOnSrcError,
  kToRelease,
};

void MediaPipe::AddMediaSrc(const sp<MediaSrc> &src) {
//set source index
  auto index = static_cast<int>(sources_.size());
  src->index_ = index;
  sources_.push_back(src);
  src->RegisterCallback([this, index](MediaSrc::MediaSrcState state, const any &extra) {
    if (state == MediaSrc::MediaSourceEnd) {
      NewEvent(kOnSrcEnd, shared_from_this(), dispatcher_)
        ->SetData("index", index)
        ->Post();
    } else if (state == MediaSrc::MediaSourceCanceled) {
      NewEvent(kOnSrcCanceled, shared_from_this(), dispatcher_)
        ->SetData("index", index)
        ->Post();
    } else if (state == MediaSrc::MediaSourceError) {
      NewEvent(kOnSrcError, shared_from_this(), dispatcher_)
        ->SetData("index", index)
        ->Post();
    }
  });
}

double MediaPipe::progress() const {
  auto min_ele_itr = std::min_element(std::begin(sources_), std::end(sources_),
                                      [](const sp<MediaSrc> &lhs, const sp<MediaSrc> &rhs)-> bool {
    return lhs->progress() < rhs->progress();
  });
  if (min_ele_itr == std::end(sources_)) return 0;
  return (*min_ele_itr)->progress();
}

bool MediaPipe::Prepare() {
  auto size = sources_.size();
  cancel_record_ = std::make_shared<DynamicBitset>(size);
  eof_record_ = std::make_shared<DynamicBitset>(size);

  for (auto &ws : sources_) {
    if (!ws->Prepare()) return false;
  }
  return true;
}

void MediaPipe::Run() {
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID("pipe");
  dispatcher_->Run();
  NewEvent(kToStart, shared_from_this(), dispatcher_)
    ->Post();
}

void MediaPipe::Cancel() {
  NewEvent(kToCancel, shared_from_this(), dispatcher_)
    ->Post();
}

void MediaPipe::Release() {
  if (dispatcher_) {
    std::promise<void> pr;
    auto future = pr.get_future();
    NewEvent(kToRelease, shared_from_this(), dispatcher_)
      ->SetData("pr", &pr)
      ->Post();
    future.get();
    dispatcher_->Release();
    dispatcher_.reset();
    LOG(VERBOSE) << "MediaPipe Released.";
  }
  state_ = kMediaPipeReleased;
}

void MediaPipe::CloseSrc() {
  for (auto &src : sources_) {
    src->Close();
  }
}

void MediaPipe::HandleEvent(const sp<Event> &event) {
  auto type = event->what();
  switch (type) {
    case kToStart: {
      PerformStart();
      break;
    }

    case kToCancel: {
      PerformCancel();
      break;
    }

    case kOnSrcEnd: {
      PerformOnEnd(event);
      break;
    }

    case kOnSrcCanceled: {
      PerformOnCanceled(event);
      break;
    }

    case kOnSrcError: {
      PerformOnError();
      break;
    }

    case kToRelease: {
      PerformRelease(event);
      break;
    }

    default:
      break;
  }
}

void MediaPipe::PerformStart() {
  for (auto &ws : sources_) {
    ws->Start();
  }
  state_ = kMediaPipeStart;
}

void MediaPipe::PerformCancel() {
  for (auto &src : sources_) {
    src->Cancel();
  }
}

void MediaPipe::PerformOnEnd(const sp<Event> &event) {
  if (state_ == kMediaPipeEnd) return;
  int index;
  {
    event->GetDataCopy("index", index);
    eof_record_->Set(index);
  }
  if (eof_record_->All()) {
    CloseSrc();
    state_ = kMediaPipeEnd;
    if (end_callback_) end_callback_();
  }
}

void MediaPipe::PerformOnCanceled(const sp<Event> &event) {
  if (state_ == kMediaPipeError || state_ == kMediaPipeCanceled) return;
  int index;
  {
    event->GetDataCopy("index", index);
    cancel_record_->Set(index);
    if (!cancel_record_->All()) return;
  }
  CloseSrc();
  if (on_err_) {
    state_ = kMediaPipeError;
    auto &src = sources_.at(index);
    if (error_callback_) error_callback_(src->error_code_);
  } else {
    state_ = kMediaPipeCanceled;
    if (cancel_callback_) cancel_callback_();
  }
}

void MediaPipe::PerformOnError() {
  if (state_ == kMediaPipeError) return;
  on_err_ = true;
  Cancel();
}

void MediaPipe::PerformRelease(const sp<Event> &event) {
  std::promise<void> *pr;
  event->GetDataCopy("pr", pr);
  pr->set_value();
  dispatcher_->Stop();
}

}
