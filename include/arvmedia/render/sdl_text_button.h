//
// Created by jerett on 16/11/18.
//

#ifndef INSPLAYER_SDL_BUTTON_H
#define INSPLAYER_SDL_BUTTON_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include "llog/llog.h"

namespace ins {


class TextButton {
  using FontRef = std::shared_ptr<TTF_Font>;
  using ClickCallback = std::function<void(TextButton &button)>;

public:
  TextButton() = default;
  TextButton(int x, int y, int w, int h, SDL_Renderer *renderer) : renderer_(renderer) {
    rect_.x = x;
    rect_.y = y;
    rect_.w = w;
    rect_.h = h;
#if _WIN32
      font_.reset(TTF_OpenFont("C:/Windows/Fonts/ARIAL.TTF", std::min(w, h)), TTF_CloseFont);
#elif __APPLE__
    font_.reset(TTF_OpenFont("/Library/Fonts/Arial.ttf", std::min(w, h)), TTF_CloseFont);
#else
//    font_.reset(TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeMono.ttf", std::min(w, h)), TTF_CloseFont);
#endif // _WIN32
    CHECK(font_ != nullptr);
  }

  ~TextButton() {
  }

  TextButton& SetText(const std::string &text) noexcept {
    text_ = text;
    return *this;
  }

  const std::string& text() const noexcept {
    return text_;
  }

  TextButton& SetOnClickCallback(const ClickCallback &callback) noexcept {
    click_callback_ = callback;
    return *this;
  }

  TextButton& SetTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
    font_color_.r = r;
    font_color_.g = g;
    font_color_.b = b;
    font_color_.a = a;
    return *this;
  }

  TextButton& SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
    bg_color_.r = r;
    bg_color_.g = g;
    bg_color_.b = b;
    bg_color_.a = a;
    return *this;
  }

  void OnEvent(const SDL_Event &e) noexcept {
    switch (e.type) {
      case SDL_MOUSEBUTTONDOWN: {
        SDL_Point point = {e.button.x, e.button.y};
        if (SDL_PointInRect(&point, &rect_)) {
          pressed_ = true;
        }
        break;
      }
      case SDL_MOUSEBUTTONUP: {
        SDL_Point point = {e.button.x, e.button.y};
        if (pressed_ && SDL_PointInRect(&point, &rect_)) {
          if (click_callback_) click_callback_(*this);
        }
        pressed_ = false;
        break;
      }
      default: {
        break;
      }
    }
  }

  void Show() noexcept {
    SDL_SetRenderDrawColor(renderer_, bg_color_.r, bg_color_.g, bg_color_.b, bg_color_.a);
    SDL_RenderFillRect(renderer_, &rect_);

    SDL_Surface *msg_surface = TTF_RenderText_Solid(font_.get(), text_.c_str(), font_color_);
    CHECK(msg_surface != nullptr) << SDL_GetError();
    SDL_Texture *msg_texture = SDL_CreateTextureFromSurface(renderer_, msg_surface);
    CHECK(msg_texture != nullptr) << SDL_GetError();

    SDL_RenderCopy(renderer_, msg_texture, nullptr, &rect_);
    SDL_FreeSurface(msg_surface);
    SDL_DestroyTexture(msg_texture);
  }

private:
  bool pressed_ = false;
  SDL_Color font_color_ = {0, 0, 0, 255};
  SDL_Color bg_color_ = {255, 255, 255, 255};
  SDL_Rect rect_;
  FontRef font_;
  SDL_Renderer *renderer_ = nullptr;
  std::string text_;
  ClickCallback click_callback_;
};

}


#endif //INSPLAYER_SDL_BUTTON_H
