//
// Created by jerett on 16/11/15.
//

#ifndef INSPLAYER_SLIDER_BAR_H
#define INSPLAYER_SLIDER_BAR_H
#include <SDL2/SDL.h>
#include <functional>

namespace ins {

class SliderBar {
public:
//  using DragCallback = std::function<void(SliderBar &slider_bar)>;
  using MouseCallback = std::function<void(SliderBar &slider_bar)>;

  SliderBar(int x, int y, int w, int h, SDL_Renderer *renderer) {
    rect_.x = x;
    rect_.y = y;
    rect_.w = w;
    rect_.h = h;
    renderer_ = renderer;
  }

  SliderBar() = default;

  SliderBar& set_max_value(double max_value) noexcept {
    max_value_ = max_value;
    return *this;
  }

  SliderBar& set_value(double value) noexcept {
    value_ = value;
    return *this;
  }

  double value() const noexcept {
    return value_;
  }

  SliderBar& set_min_value(double min_value) noexcept {
    min_value_ = min_value;
    return *this;
  }

  SliderBar& set_fill_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a) noexcept {
    fill_color_ = {r, g, b, a};
    return *this;
  }

  SliderBar& set_border_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a) noexcept {
    border_color_ = {r, g, b, a};
    return *this;
  }

  void Show() noexcept {
    //fill first
    SDL_SetRenderDrawColor(renderer_, fill_color_.r, fill_color_.g, fill_color_.b, fill_color_.a);
    SDL_Rect fill_rect = {rect_.x,
                          rect_.y,
                          static_cast<int>(rect_.w*std::min(1.0, value_ / max_value_)),
                          rect_.h};
    SDL_RenderFillRect(renderer_, &fill_rect);

    //draw border
    SDL_SetRenderDrawColor(renderer_, border_color_.r, border_color_.g, border_color_.b, border_color_.a);
    SDL_RenderDrawRect(renderer_, &rect_);
  }

  SliderBar& SetOnDragCallback(const MouseCallback &callback) noexcept {
    drag_callback_ = callback;
    return *this;
  }

  SliderBar& SetOnMouseUpCallback(const MouseCallback &callback) noexcept {
    mouse_up_callback_ = callback;
    return *this;
  }

  SliderBar& SetOnMouseDownCallback(const MouseCallback &callback) noexcept {
    mouse_down_callback_ = callback;
    return *this;
  }

  void OnEvent(const SDL_Event &e) noexcept {
    switch (e.type) {
      case SDL_MOUSEMOTION: {
        int x = e.motion.x;
        if (e.motion.x < rect_.x) x = rect_.x;
        if (e.motion.x > rect_.x + rect_.w) x = rect_.x + rect_.w;
        if (pressed_) {
//          LOG(INFO) << "mouse motion";
          value_ = static_cast<float>(x - rect_.x) / rect_.w * (max_value_ - min_value_) + min_value_;
          if (drag_callback_) {
            drag_callback_(*this);
          }
        }
        break;
      }
      case SDL_MOUSEBUTTONDOWN: {
        SDL_Point point = {e.button.x, e.button.y};
        if (SDL_PointInRect(&point, &rect_)) {
//          LOG(INFO) << "mouse down";
          pressed_ = true;
          value_ = static_cast<float>(e.motion.x - rect_.x) / rect_.w * (max_value_ - min_value_) + min_value_;
          if (mouse_down_callback_) mouse_down_callback_(*this);
        }
        break;
      }
      case SDL_MOUSEBUTTONUP: {
        if (pressed_) {
          if (mouse_up_callback_) mouse_up_callback_(*this);
          pressed_ = false;
        }
        break;
      }
      default: {
        break;
      }
    }
  }

private:
  SDL_Rect rect_;
  double max_value_;
  double value_;
  double min_value_;
  bool pressed_ = false;
  SDL_Renderer *renderer_ = nullptr;
  SDL_Color fill_color_;
  SDL_Color border_color_;
  MouseCallback drag_callback_;
  MouseCallback mouse_down_callback_;
  MouseCallback mouse_up_callback_;
};

}


#endif //INSPLAYER_SLIDER_BAR_H
