//
// Created by jerett on 16/1/25.
//

#ifndef EVENTLOOP_EVENT_DISPATCHER_H
#define EVENTLOOP_EVENT_DISPATCHER_H

extern "C" {
#include <event2/event.h>
}
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <string>
#include <list>

struct event_base;
struct event;

namespace ins {

class Event;
class EventHandler;

class EventDispatcher {
  friend class Event;
public:
  EventDispatcher();

  virtual ~EventDispatcher();

  void Run() noexcept;

  /**
   * 如果在事件循环中调用，同步退出事件循环，
   * 否则，异步退出，
   */
  void Stop() noexcept;

  /*
   * 必须先Stop
   * 不能在事件循环中Release，否则会死锁!
   */
  void Release() noexcept;

  void SetID(const std::string &id) noexcept {
    id_ = id;
  }

  bool running() const noexcept {
    return running_;
  }

private:
  void Register(const std::shared_ptr<Event> &event) noexcept;
  void UnRegister(const std::shared_ptr<Event> &event) noexcept;

  void DispatchEvent(const std::shared_ptr<Event> &event) noexcept;
  static void OnLoopEvent(evutil_socket_t sock, short what, void *arg);
  static void OnTimerEvent(evutil_socket_t sock, short what, void *arg);
  static void OnQuitEvent(evutil_socket_t sock, short what, void *arg);

private:
  using TimerKey = std::pair<EventDispatcher *, std::shared_ptr<Event>>;
  using BreakEventKey = std::pair<EventDispatcher *, struct event *>;
  void FreeTimerEvent(const std::unordered_map<TimerKey *, struct event *>::iterator &itr);

private:
  std::mutex events_mtx_;
  std::list<std::shared_ptr<Event>> events_;
  std::unordered_map<TimerKey *, struct event *> timer_events_;

  event_base *event_base_ = nullptr;
  struct event *notify_event_ = nullptr;
  std::thread event_thread_;
  std::atomic_bool running_ = {false};
  std::string id_;
};


}


#endif //EVENTLOOP_EVENT_DISPATCHER_H
