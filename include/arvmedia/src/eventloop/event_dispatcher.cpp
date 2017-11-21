//
// Created by jerett on 16/1/25.
//

#include "event_dispatcher.h"

extern "C" {
#include <event2/thread.h>
}
#include <algorithm>
#include <llog/llog.h>
#include "event.h"
#include "event_handler.h"

namespace ins {
  
void EventDispatcher::OnLoopEvent(evutil_socket_t sock, short what, void *arg) {
  Ignore(sock);
  Ignore(what);
  using namespace std::chrono;

  EventDispatcher *dispatcher = reinterpret_cast<EventDispatcher*>(arg);
  while (dispatcher->running()) {
    std::shared_ptr<Event> event;
    std::unique_lock<std::mutex> lck(dispatcher->events_mtx_);
    {
      if (dispatcher->events_.empty()) break;
      event = dispatcher->events_.front();
      dispatcher->events_.pop_front();
    }

    if (!event->delay()) {
      lck.unlock();
      dispatcher->DispatchEvent(event);
    } else {
      auto pass = duration_cast<microseconds>(event->execute_time_ - steady_clock::now()).count();
      auto us = std::max<microseconds::rep>(0, pass);

      struct timeval timeout = { static_cast<decltype(timeout.tv_sec)>(us / 1000000),
                                static_cast<decltype(timeout.tv_usec)>(us % 1000000) };
      TimerKey *timer_key = new TimerKey;
      timer_key->first = dispatcher;
      timer_key->second = event;
      struct event *timer_event = evtimer_new(dispatcher->event_base_, OnTimerEvent, timer_key);

      evtimer_add(timer_event, &timeout);
      event_priority_set(timer_event, 1);
      event->extra_data_ = timer_key;
      dispatcher->timer_events_.insert({timer_key, timer_event});
    }
  }
}

void EventDispatcher::OnTimerEvent(evutil_socket_t sock, short what, void *arg) {
  Ignore(sock);
  Ignore(what);

  TimerKey *key = reinterpret_cast<TimerKey *>(arg);
  auto dispatcher = key->first;
  std::shared_ptr<Event> event = key->second;

  {
    std::lock_guard<std::mutex> lck(dispatcher->events_mtx_);
    key->second->extra_data_ = nullptr;
    auto itr = dispatcher->timer_events_.find(key);
    CHECK(itr != dispatcher->timer_events_.end());
    dispatcher->FreeTimerEvent(itr);
    dispatcher->timer_events_.erase(itr);
  }

  dispatcher->DispatchEvent(event);
}

void EventDispatcher::OnQuitEvent(evutil_socket_t sock, short what, void *arg) {
  Ignore(sock);
  Ignore(what);

  BreakEventKey *break_event_key = reinterpret_cast<BreakEventKey *>(arg);
  auto dispatcher = break_event_key->first;
  event_free(break_event_key->second);
  LOG(VERBOSE) << "[" << dispatcher->id_ << "]"
               << "event_base_loopbreak: "
               << event_base_loopbreak(dispatcher->event_base_);
  delete break_event_key;
}

void EventDispatcher::DispatchEvent(const std::shared_ptr<Event> &event) noexcept {
  auto sp_handler = event->handler_.lock();
  if (sp_handler) {
    sp_handler->HandleEvent(event);
  } else {
    LOG(VERBOSE) << "invalid handler session ";
  }
}

EventDispatcher::EventDispatcher() {
  static std::once_flag flag;
  std::call_once(flag, [](){
#if (_WIN32)
    LOG(VERBOSE) << "libevent use phreads:" << evthread_use_windows_threads();
#else
    LOG(VERBOSE) << "libevent use phreads:" << evthread_use_pthreads();
#endif
  });
}

void EventDispatcher::Run() noexcept {
  event_base_ = event_base_new();
  notify_event_ = event_new(event_base_, -1, EV_PERSIST | EV_READ, OnLoopEvent, this);
  event_add(notify_event_, nullptr);
  CHECK(event_base_ != nullptr) << "event base new failure";
  auto loop_func = [&]() {
    event_base_dispatch(event_base_);
    LOG(VERBOSE) << "[" << id_ << "]"<< " thread exit ..." ;
  };
  event_thread_ = std::thread(loop_func);
  running_ = true;
}

void EventDispatcher::Stop() noexcept {
  if (!running_) return;

  running_ = false;
  if (std::this_thread::get_id() == event_thread_.get_id()) {
    LOG(VERBOSE) << "[" << id_ << "]" << " event break event:" << event_base_loopbreak(event_base_);
  } else {
    BreakEventKey *break_event_key = new BreakEventKey;
    struct event *ev = evtimer_new(event_base_, OnQuitEvent, break_event_key);
    event_priority_set(ev, 100);
    break_event_key->first = this;
    break_event_key->second = ev;
    struct timeval timeout = {0, 0};
    evtimer_add(ev, &timeout);
  }
}

void EventDispatcher::Release() noexcept {
  if (event_thread_.joinable()) event_thread_.join();

  {
    std::lock_guard<std::mutex> lck(events_mtx_);
    events_.clear();

    for (auto itr = timer_events_.begin(); itr != timer_events_.end(); ++itr) {
      FreeTimerEvent(itr);
    }
    timer_events_.clear();
  }

  if (event_base_) {
    event_free(notify_event_);
    event_base_free(event_base_);
    event_base_ = nullptr;
  }
  LOG(VERBOSE) << "[" << id_  << "]" << " dispatcher release";
}

void EventDispatcher::Register(const std::shared_ptr<Event> &event) noexcept {
  if (!running_) {
    LOG(ERROR) << "try to regsiter when not runnig:" << id_ << " what:" << event->what();
    return;
  }
  Timer timer;
  std::lock_guard<std::mutex> lck(events_mtx_);
  events_.push_back(event);
  event_active(notify_event_, EV_READ, 0);
}

void EventDispatcher::UnRegister(const std::shared_ptr<Event> &event) noexcept {
  std::lock_guard<std::mutex> lck(events_mtx_);
  //probe list first
  auto itr = std::find(events_.begin(), events_.end(), event);
  if (itr == events_.end() && !event->delay()) {
    // no delay event not found in list
    LOG(VERBOSE) << "cancel no delay event failed. type:" << event->what() << "not found in queue";
    return;
  } else if (itr == events_.end() && event->delay()) {
    //probe timer event
    TimerKey *timer_key = reinterpret_cast<TimerKey *>(event->extra_data_);
    if (timer_key == nullptr) {
      LOG(VERBOSE) << "cancel delay event failed. extra data not set";
      return;
    } else {
      auto itr = timer_events_.find(timer_key);
      CHECK(itr != timer_events_.end()) << "cancel delay event failed. extra data set but timer not found. please check logic";
      FreeTimerEvent(itr);
      timer_events_.erase(itr);
      LOG(VERBOSE) << "cancel delay event success.";
    }
  } else {
    //found no-delay event in list.
    events_.erase(itr);
    LOG(VERBOSE) << "cancel no-delay event success.";
  }
}

void EventDispatcher::FreeTimerEvent(const std::unordered_map<TimerKey *, struct event *>::iterator &itr) {
  auto timer_key = itr->first;
  timer_key->second->extra_data_ = nullptr;
  auto timer_event = itr->second;
  event_free(timer_event);
  delete timer_key;
}


EventDispatcher::~EventDispatcher() {
}
  
}
