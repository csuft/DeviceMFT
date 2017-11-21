//
// Created by jerett on 16/1/25.
//

#ifndef EVENTLOOP_EVENT_HANDLER_H
#define EVENTLOOP_EVENT_HANDLER_H

#include <memory>

namespace ins {
class Event;
class EventDispatcher;
    
class EventHandler {
  friend class EventDispatcher;
public:
  virtual ~EventHandler() = default;

protected:
  virtual void HandleEvent(const std::shared_ptr<Event> &event) = 0;
};

}


#endif //EVENTLOOP_EVENT_HANDLER_H
