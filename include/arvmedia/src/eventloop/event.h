//
// Created by jerett on 16/1/25.
//

#ifndef EVENTLOOP_EVENT_H
#define EVENTLOOP_EVENT_H

#include <stdint.h>
#include <memory>
#include <iostream>
#include <chrono>
#include <unordered_map>

//#include <boost/any.hpp>

#include <llog/llog.h>
#include <av_toolbox/any.h>
#include <av_toolbox/sp.h>

namespace ins {
class EventHandler;
class EventDispatcher;
class Event;

template <typename T>
struct IdType {
  typedef T type;
};

//template <typename T>
//using sp = std::shared_ptr<T>;

sp<Event> NewEvent(uint32_t what, const sp<EventHandler> &handler, const sp<EventDispatcher> &dispatcher);

class Event :public std::enable_shared_from_this<Event> {
  friend class EventDispatcher;
public:
  explicit Event(uint32_t what) : what_(what)
  {
  }

  template <typename T>
  sp<Event> SetData(const std::string &key, T &&val) noexcept(false){
    data_[key] = any(std::forward<T>(val));
    return shared_from_this();
  }

  template <typename T>
  T* GetDataNoCopy(const std::string& key) noexcept(false){
    auto itr = data_.find(key);
    if (itr == data_.end()) {
      LOG(INFO) << "unable to find key: " << key ;
      return nullptr;
    }
    auto &&val = any_cast<T&>(itr->second);
    return &val;
  }

  template <typename T>
  bool GetDataCopy(const std::string& key, T &val) const noexcept(false) {
    auto itr = data_.find(key);
    if (itr == data_.end()) {
      LOG(INFO) << "unable to find key: " << key ;
      return false;
    }
    val = any_cast<const T&>(itr->second);
    return true;
  }

  sp<Event> Clone() const {
    auto event = std::make_shared<Event>(what_);
    event->handler_ = handler_;
    event->dispatcher_ = dispatcher_;
    event->data_ = data_;
    return event;
  }

  sp<Event> set_handler(const sp<EventHandler> &handler) noexcept {
    handler_ = handler;
    return shared_from_this();
  }

  sp<Event> set_dispatcher(const sp<EventDispatcher> &dispatcher) noexcept {
    dispatcher_ = dispatcher;
    return shared_from_this();
  }

  void Post() noexcept;

  void PostWithDelay(int delay_ms) noexcept;

  //取消Post还未执行的通知，必须已经Post，否则可能crash
//  void Cancel() noexcept;

  int delay_ms() const noexcept {
    return delay_ms_;
  }

  bool delay() const noexcept {
    return delay_ms_ > 0;
  }

  void set_what(uint32_t what) {
    what_ = what;
  }
    
  uint32_t what() const noexcept {
    return what_;
  }

  void ClearData() noexcept {
    data_.clear();
  }

  ~Event() {
//    LOG(INFO) << "~Event what:" << what_;
  }

private:
  void set_delay_ms(int delay_ms) noexcept {
    delay_ms_ = delay_ms;
  }

//  using any = boost::any;
  using any = ins::any;

private:
  void *extra_data_ = nullptr;
  uint32_t what_ = 0;
  int delay_ms_ = 0;
  std::chrono::steady_clock::time_point execute_time_;
  std::weak_ptr<ins::EventHandler> handler_;
  std::weak_ptr<ins::EventDispatcher> dispatcher_;
  std::unordered_map<std::string, any> data_;
};

std::shared_ptr<Event> CloneEventAndAlterType(const std::shared_ptr<Event> &event, uint32_t what);


}



#endif //EVENTLOOP_EVENT_H
