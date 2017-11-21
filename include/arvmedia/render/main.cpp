//
// Created by jerett on 16-6-14.
//


extern "C" {
#include <libswscale/swscale.h>
}
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <llog/llog.h>
#include <av_toolbox/threadsafe_queue.h>
#include <av_toolbox/scaler.h>
#include <av_toolbox/ffmpeg_util.h>
#include "player/player.h"
#include "sdl_slider_bar.h"
#include "sdl_text_button.h"
#define SDL_main main

const static int SDL_Player_OnPreapred = SDL_USEREVENT + 1;

using namespace ins;
sp<ins::Player> player;
bool prepared = false;
sp<ins::Scaler> scaler;
ins::Player::PlayerStatus  player_status;
ThreadSafeQueue<sp<AVFrame>> img_queue_buffer;
SDL_Renderer *renderer = nullptr;
SDL_Texture *img_tex = nullptr;
SDL_Window *player_window = nullptr;
ins::SliderBar progress_bar;
ins::TextButton button;

bool quit = false;
void SetUpTexture();

void StatusChanged(ins::Player::NotifyType type, int val, int extra) {

  if (type == ins::Player::kNotifyBufferingProgress) {
    button.SetText("buffering:" + std::to_string(val));
  }

  if (type == ins::Player::kNotifyStatus) {
    player_status = static_cast<ins::Player::PlayerStatus>(val);
    switch (player_status) {
      case ins::Player::kStatusUnKnown:
      {
        LOG(INFO) << "status -> unknown";
        break;
      }

      case ins::Player::kStatusPrepared:
      {
        LOG(INFO) << "status -> prepared";
        SDL_Event event;
        event.type = SDL_Player_OnPreapred;
        SDL_PushEvent(&event);
        break;
      }

      case ins::Player::kStatusPlaying:
      {
        LOG(INFO) << "status -> playing";
        button.SetText("Pause");
        //player->SeekAsync(300*1000);
        break;
      }

      case ins::Player::kStatusBuffering:
      {
        LOG(INFO) << "status -> buffering";
        button.SetText("buffering");
        break;
      }

      case ins::Player::kStatusPaused:
      {
        LOG(INFO) << "status -> paused";
        button.SetText("Play");
        break;
      }

      case ins::Player::kStatusEnd:
      {
        LOG(INFO) << "status -> end";
        button.SetText("Play");
        break;
      }

      case ins::Player::kStatusFailed:
      {
        LOG(INFO) << "status -> failed";
        break;
      }

      default:
      {
        break;
      }
    }
  }
 
}

void SetUpTexture() {
//  Uint32 pixel_format = IsUsingFFmpegDecoder() ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_NV12;
  scaler.reset(new Scaler(player->GetVideoWidth(), player->GetVideoHeight(), AV_PIX_FMT_YUV420P, SWS_BILINEAR));
  Uint32 pixel_format = SDL_PIXELFORMAT_YV12;
  //Uint32 pixel_format = SDL_PIXELFORMAT_NV12;
  LOG(VERBOSE) << " width:" << player->GetVideoWidth()
    << " height:" << player->GetVideoHeight();
  img_tex = SDL_CreateTexture(renderer,
                              pixel_format,
                              SDL_TEXTUREACCESS_STREAMING,
                              player->GetVideoWidth(),
                              player->GetVideoHeight());
  if (img_tex == nullptr) {
    LOG(ERROR) << "error Create tex:" << SDL_GetError();
  }
  CHECK(img_tex != nullptr);
}

void OnPrepared() {
  progress_bar.set_max_value(player->GetDuration())
    .set_min_value(0.);
  if (player->HasVideo()) {
    SetUpTexture();
    SDL_SetWindowSize(player_window, 1200, 1200 * player->GetVideoHeight() / player->GetVideoWidth());
  } else {
    SDL_SetWindowSize(player_window, 1200, 600);
  }
  prepared = true;
}

void RenderVideoToolboxImage(const sp<AVFrame> &img) {
//  LOG(INFO) << "..............";
#if (__APPLE__)
  int size = img->width * img->height/2*3;
  std::unique_ptr<uint8_t []> nv12_data(new uint8_t[size]);
  uint8_t *dst_y_plane = nv12_data.get();
  uint8_t *dst_nv_plane = nv12_data.get() + img->width * img->height;

  CVPixelBufferRef buffer = reinterpret_cast<CVPixelBufferRef >(img->data[3]);
  CVPixelBufferLockBaseAddress(buffer, 0);
  auto src_y_plane = CVPixelBufferGetBaseAddressOfPlane(buffer, 0);
  auto src_uv_plane = CVPixelBufferGetBaseAddressOfPlane(buffer, 1);
  memcpy(dst_y_plane, src_y_plane, img->width * img->height);
  memcpy(dst_nv_plane, src_uv_plane, img->width * img->height/2);
  CVPixelBufferUnlockBaseAddress(buffer, 0);

  auto ret = SDL_UpdateTexture(img_tex, nullptr, nv12_data.get(), img->width);
  if (ret == -1) {
    LOG(ERROR) << "invalid texture .";
  }
#endif
}

void RenderNV12Image(const sp<AVFrame> &img) {
  int size = img->width * img->height/2*3;
  std::unique_ptr<uint8_t []> nv12_data(new uint8_t[size]);
  uint8_t *y_plane = nv12_data.get();
  uint8_t *nv_plane = nv12_data.get() + img->width * img->height;
  memcpy(y_plane, img->data[0], img->width * img->height);
  memcpy(nv_plane, img->data[1], img->width * img->height/2);

  auto ret = SDL_UpdateTexture(img_tex, nullptr, nv12_data.get(), img->linesize[0]);
  if (ret == -1) {
    LOG(ERROR) << "invalid texture .";
  }
}

void RenderYUVImage(const sp<AVFrame> &img) {
  auto ret = SDL_UpdateYUVTexture(img_tex, nullptr,
                                  img->data[0], img->linesize[0],
                                  img->data[1], img->linesize[1],
                                  img->data[2], img->linesize[2]);
  if (ret == -1) {
    LOG(ERROR) << "invalid texture .";
  }
}

int main(int argc, char *argv[]) {
  LOG(INFO) << "sdl init" << SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
  LOG(INFO) << "ttf init" << TTF_Init();
  player_window = SDL_CreateWindow("insplayer", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, 1200, 600, SDL_WINDOW_OPENGL);
  CHECK(player_window != nullptr);
  auto gl_context = SDL_GL_CreateContext(player_window);
  renderer = SDL_CreateRenderer(player_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  CHECK(renderer != nullptr);

//  player.reset(new ins::Player("http://192.168.2.123:8080/video/car.mp4"));
//  player.reset(new ins::Player("http://files.selfimg.com.cn/media/video/2016/04/21/c033bf2156f84b3e0bf4f538941f56bd.mp4"));
//  player.reset(new ins::Player("file:///Users/jerett/Desktop/shenda.insv"));
  if (argc < 2) {
    LOG(ERROR) << "no path for play";
    return -1;
  }
  player.reset(new ins::Player(argv[1]));
//  player->SetOption("zero_latency", "true");
//  player->SetOption("buffer_ms", "3000");
//  auto max_buffer_size = 300 * 1024;
//  player->SetOption("max_buffer_size", std::to_string(max_buffer_size));

  player->SetVideoRenderer([&](const sp<AVFrame> &img) {
    img_queue_buffer.push(img);
//    LOG(INFO) << "display ...";
  });

  player->SetStatusNotify(StatusChanged);
  player->PrepareAsync();
  player->PlayAsync();

  progress_bar = ins::SliderBar(300, 550, 600, 30, renderer);
  progress_bar.set_border_color(255, 255, 255, 255)
      .set_fill_color(255, 255, 0, 100);

  auto update_progress = true;
  progress_bar.SetOnMouseDownCallback([&](ins::SliderBar &slider) {
    update_progress = false;
    LOG(INFO) << "Mouse Down..........";
  });

  progress_bar.SetOnDragCallback([&](ins::SliderBar &slider) {
    if (player_status == ins::Player::kStatusPaused) {
      //player->SeekAsync(slider.value(), true);
      player->SeekAsync(slider.value(), true);
    }
  });

  progress_bar.SetOnMouseUpCallback([&](ins::SliderBar &slider) {
    update_progress = true;
//    if (player_status == ins::Player::kStatusPlaying || player_status == ins::Player::kStatusEnd) {
      player->SeekAsync(slider.value(), true);
//    }
  });

  button = ins::TextButton(200, 550, 60, 30, renderer);
  button.SetText("Pause")
      .SetOnClickCallback([](ins::TextButton &button) {
        if (button.text() == "Pause") {
          player->PauseAsync();
        } else if (button.text() == "Play") {
          player->PlayAsync();
        }
        LOG(INFO) << "click ...";
      });

  Timer fps_timer;
  auto frame_cnt = 0;

  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      switch (event.type) {
        case SDL_QUIT:
        {
          player->ReleaseSync();
          quit = true;
          break;
        }

        case SDL_Player_OnPreapred:
        {
          OnPrepared();
          break;
        }

        default:
        {
          break;
        }
      }
      
      //dispatch event
      {
        progress_bar.OnEvent(event);
        button.OnEvent(event);
      }
    }

    if (quit) break;
    //Render video
    {
      sp<AVFrame> img;
      img_queue_buffer.try_pop(img);

      if (img) {
        //if (img->format == AV_PIX_FMT_YUV420P) {
        //  RenderYUVImage(img);
        //} else if (img->format == AV_PIX_FMT_NV12) {
        //  RenderNV12Image(img);
        //} else if (img->format == AV_PIX_FMT_VIDEOTOOLBOX) {
        //  RenderVideoToolboxImage(img);
        //}
        ++frame_cnt;
        if (fps_timer.Pass() >= 2000) {
          auto fps = frame_cnt * 1000 / fps_timer.Pass();
          LOG(VERBOSE) << "player fps:" << fps;
          fps_timer.Reset();
          frame_cnt = 0;
        }

        if (img->format == AV_PIX_FMT_YUV420P) {
          RenderYUVImage(img);
        } else {
          //RenderVideoToolboxImage(img);
          sp<AVFrame> yuv_frame;
          scaler->Init(img);
          scaler->ScaleFrame(img, yuv_frame);
          RenderYUVImage(yuv_frame);
        }
      }
    }

    //do render 
    {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      if (prepared) {
        if (update_progress) progress_bar.set_value(player->CurrentPosition());
        if (img_tex) SDL_RenderCopy(renderer, img_tex, nullptr, nullptr);
        progress_bar.Show();
        button.Show();
      }
      SDL_RenderPresent(renderer);
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(player_window);
#if _WIN32
  system("pause");
#endif
  return 0;
}

