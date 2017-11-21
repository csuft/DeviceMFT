//
// Created by jerett on 16/1/25.
//

#include "event.h"
#include "event_handler.h"
#include "event_dispatcher.h"
#include "llog/llog.h"

namespace ins {

sp<Event> NewEvent(uint32_t what, const sp<EventHandler> &handler, const sp<EventDispatcher> &dispatcher) {
  auto event = std::make_shared<Event>(what);
  event->set_handler(handler);
  event->set_dispatcher(dispatcher);
  return event;
}

sp<Event> CloneEventAndAlterType(const std::shared_ptr<Event> &event, uint32_t what) {
  auto event_dup = event->Clone();
  event_dup->set_what(what);
  return event_dup;
}

void Event::Post() noexcept {
  PostWithDelay(0);
}

void Event::PostWithDelay(int delay_ms) noexcept {
  set_delay_ms(delay_ms);
  execute_time_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
  auto sp_dispatcher = dispatcher_.lock();

  if (sp_dispatcher) {
    sp_dispatcher->Register(shared_from_this());
  } else {
    LOG(INFO) << "weak dispatcher is invalid now";
  }
}

//void Event::Cancel() noexcept {
//  auto sp_dispatcher = dispatcher_.lock();
//
//  if (sp_dispatcher) {
//    sp_dispatcher->UnRegister(shared_from_this());
//  } else {
//    LOG(INFO) << "weak dispatcher is invalid now";
//  }
//}

}
