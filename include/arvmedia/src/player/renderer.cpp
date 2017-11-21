#include "renderer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}
#include <future>
#include <eventloop/event_dispatcher.h>
#include <eventloop/event.h>
#include <av_toolbox/ffmpeg_util.h>
#include "audioplay/audio_player.h"
#include "audio_transform/audio_resample_transform.h"
#include "system_clock.h"

namespace ins {

enum {
  kToStart,
  kToPause,
  kToResume,
  kToRender,
  kToLoop,
  kToClear,
};

Renderer::Renderer(const AVStream *video_stream,
                   const AVStream *audio_stream,
                   MediaType media_type,
                   const sp<Event>& notify) noexcept
  : media_type_(media_type), video_stream_(video_stream), audio_stream_(audio_stream), notify_(notify) {
  notify_->SetData("serial_num", serial_num_);
  notify_->SetData("renderer", this);
  dispatcher_ = std::make_shared<EventDispatcher>();
  dispatcher_->SetID("Renderer");
}

Renderer::~Renderer() noexcept = default;

bool Renderer::Prepare() {
  ResetEOFStatus();

  if (audio_stream_) {
    AudioTransformContext ctx;
    ctx.in_stream = audio_stream_;
    auto codecpar = NewAVCodecParameters();
    avcodec_parameters_copy(codecpar.get(), audio_stream_->codecpar);
    ctx.out_codecpar = codecpar;
    auto entry = av_dict_get(audio_stream_->metadata, "spatial_audio", nullptr, 0);
    if (entry != nullptr) {
      if (strcmp(entry->value, "1") == 0) {
        ctx.spatial_audio = true;
        LOG(INFO) << "#################is spatial audio";
      }
    } else {
      LOG(INFO) << "#################not spatial audio";
    }

    sp<AudioTransform> last_audio_transform = audio_transform_;
    while (last_audio_transform && last_audio_transform->next_transform()) {
      last_audio_transform = last_audio_transform->next_transform();
    }
    auto out_sample_rate_ = audio_stream_->codecpar->sample_rate;
    const AVSampleFormat out_fmt_ = AV_SAMPLE_FMT_S16;
    const uint64_t out_ch_layout_ = AV_CH_LAYOUT_STEREO;
    auto resample_transform = std::make_shared<AudioResampleTransform>(out_ch_layout_, out_fmt_, out_sample_rate_);
    if (last_audio_transform == nullptr) {
      audio_transform_ = resample_transform;
    } else {
      last_audio_transform->set_next_transform(resample_transform);
    }
    if (audio_transform_ && !audio_transform_->Init(&ctx)) return false;

    //sync to audio
    auto sample_rate = ctx.out_codecpar->sample_rate;
    auto channels = ctx.out_codecpar->channels;
    auto channel_layout = ctx.out_codecpar->channel_layout;
    char channel_layout_str[256];
    av_get_channel_layout_string(channel_layout_str, sizeof(channel_layout_str), channels, channel_layout);

    auto sample_fmt = static_cast<AVSampleFormat>(audio_stream_->codecpar->format);
    LOG(INFO) << " using audio clock."
              << " sample_fmt:" << av_get_sample_fmt_name(sample_fmt)
              << " sample size:" << av_get_bytes_per_sample(sample_fmt)
              << " sample_rate:" << sample_rate
              << " channel:" << channels
              << " layout:" << channel_layout_str;
    audio_player_ = ins::SuitableAudioPlayer(sample_rate, channels,
                                             [&]()->sp<AVFrame> {
      return PlayAudio();
    });
    media_clock_ = audio_player_;
  } else {
    LOG(INFO) << "use System clock.";
    //sync to system clock
    media_clock_.reset(new SystemClock());
  }
  return true;
}

void Renderer::StartAsync() {
  dispatcher_->Run();
  NewEvent(kToStart, shared_from_this(), dispatcher_)->Post();
}

void Renderer::PauseSync() {
  std::promise<void> pr;
  auto future = pr.get_future();
  NewEvent(kToPause, shared_from_this(), dispatcher_)
    ->SetData("pr", &pr)
    ->Post();
  future.get();
}

void Renderer::RenderVideoSync(const sp<AVFrame>& frame) {
  std::promise<void> pr;
  auto future = pr.get_future();
  NewEvent(kToRender, shared_from_this(), dispatcher_)
    ->SetData("pr", &pr)
    ->SetData("frame", frame)
    ->Post();
  future.get();
}

void Renderer::ResumeSync() {
  std::promise<void> pr;
  auto future = pr.get_future();
  NewEvent(kToResume, shared_from_this(), dispatcher_)
    ->SetData("pr", &pr)
    ->Post();
  future.get();
}

void Renderer::ReleaseSync() {
  dispatcher_->Stop();
  dispatcher_->Release();
  media_clock_.reset();
  audio_player_.reset();
  ClearResource();
}

bool Renderer::CheckEvent(const sp<Event>& event) const {
  int64_t serial_num;
  Renderer *renderer = nullptr;
  if (!event->GetDataCopy("serial_num", serial_num)) return false;
  if (!event->GetDataCopy("renderer", renderer)) return false;
  return renderer == this && serial_num <= serial_num_;
}

bool Renderer::VideoBufferIsFull() const {
  return img_buffer_.size() == kMaxImgBufferSize;
}

bool Renderer::AudioBufferIsFull() const {
  return pcm_buffer_.size() == kMaxPCMBufferSize;
}

int64_t Renderer::CurrentPosition() const {
  if (media_type_ == kRealtimeStreamMedia || !media_clock_) return -1;
  return media_clock_->NowInMs();
}

void Renderer::SeekSync(int64_t position_ms) {
  std::promise<void> pr;
  auto future = pr.get_future();
  NewEvent(kToClear, shared_from_this(), dispatcher_)
    ->SetData("pr", &pr)
    ->SetData("position", position_ms)
    ->Post();
  future.get();
}

void Renderer::EnqueueVideo(const sp<AVFrame> &frame) {
  img_buffer_.push(frame);
}

void Renderer::EnqueueAudio(const sp<AVFrame> &frame) {
//  LOG(VERBOSE) << "enqueue audio:" << frame->pts;
  pcm_buffer_.push(frame);
}

void Renderer::HandleEvent(const sp<Event> &event) {
  switch (event->what()) {
    case kToStart:
    {
      Start(event);
      break;
    }

    case kToPause:
    {
      Pause(event);
      break;
    }

    case kToResume:
    {
      Resume(event);
      break;
    }

    case kToRender:
    {
      RenderVideo(event);
      break;
    }

    case kToLoop:
    {
      OnLoop(event);
      break;
    }

    case kToClear:
    {
      Seek(event);
      break;
    }

    default:
    {
      break;
    }
  }
}

void Renderer::Start(const sp<Event> &event) {
  if (audio_player_) audio_player_->StartPlay();
  if (video_stream_) {
    media_clock_->Start(TimestampToMs(video_stream_->first_dts, video_stream_->time_base));
  } else {
    media_clock_->Start(TimestampToMs(audio_stream_->first_dts, audio_stream_->time_base));
  }
  NewEvent(kToLoop, shared_from_this(), dispatcher_)->Post();
}

void Renderer::Pause(const sp<Event> &event) {
  {
    if (audio_player_) audio_player_->PausePlay();
    media_clock_->Pause();
  }

  {
    std::promise<void> *pr;
    event->GetDataCopy("pr", pr);
    pr->set_value();
  }
}

void Renderer::Resume(const sp<Event> &event) {
  {
    if (audio_player_) audio_player_->ResumePlay();
    media_clock_->Resume();
  }

  {
    std::promise<void> *pr;
    event->GetDataCopy("pr", pr);
    pr->set_value();
  }
}

void Renderer::OnLoop(const sp<Event> &event) {
  event->PostWithDelay(15);
  {
    //render video
    sp<AVFrame> video_img;
    if (img_buffer_.try_peek(video_img)) {
      if (media_type_ == kRealtimeStreamMedia) {
        /**
        * render directly
        **/
        img_buffer_.try_pop();
      } else {
        /**
        * sync to system
        */
        if (video_img->pts == AV_NOPTS_VALUE) {
          //LOG(INFO) << "none pts_ms , using next video pts instead";
          video_img->pts = next_video_pts_;
        }
        auto framerate = static_cast<double>(video_stream_->avg_frame_rate.num) / video_stream_->avg_frame_rate.den;
        next_video_pts_ = video_img->pts + static_cast<int64_t>(1000.0 / framerate);
        auto diff_ms = video_img->pts - media_clock_->NowInMs();
        if (diff_ms >= 30) {
          //LOG(INFO) << "invalid img:" << video_img->pts << "  " << media_clock_->NowInMs();
          video_img = nullptr;
        } else {
//          LOG(INFO) << "display img:" << video_img->pts << "  " << media_clock_->NowInMs();
          img_buffer_.try_pop();
        }
      }
    }

    if (renderer_ && video_img) {
      renderer_(video_img);
    }
  }

  {
    //check video end
    if (video_in_eof_  && img_buffer_.empty() && !video_out_eof_) {
      LOG(VERBOSE) << "video out eof.";
      video_out_eof_ = true;
    }

    //check audio end
    if (audio_in_eof_ && pcm_buffer_.empty() && !audio_out_eof_) {
      LOG(VERBOSE) << "audio out eof.";
      audio_out_eof_ = true;
      if (!video_out_eof_) {
        //change audio clock to system clock
        audio_player_->ChangeToSystemClock();
      }
    }

    //check play end
    if (video_out_eof_ && audio_out_eof_ && !eof_) {
      LOG(VERBOSE) << "check play to end";
      eof_ = true;
      video_out_eof_ = true;
      audio_out_eof_ = true;
      UpdateSerialNum();
      CloneEventAndAlterType(notify_, kRenderEof)->Post();
    }
  }
}

void Renderer::ResetEOFStatus() {
  video_in_eof_ = true;
  video_out_eof_ = true;
  audio_in_eof_ = true;
  audio_out_eof_ = true;
  eof_ = false;

  if (video_stream_) {
    video_in_eof_ = false;
    video_out_eof_ = false;
  }

  if (audio_stream_) {
    audio_in_eof_ = false;
    audio_out_eof_ = false;
  }
}

void Renderer::Seek(const sp<Event> &event) {
  {
    ResetEOFStatus();
    ClearResource();
    serial_num_ = 0;

    if (audio_stream_) audio_player_->ChnageToAudioClock();
    int64_t position_ms;
    event->GetDataCopy("position", position_ms);
    media_clock_->ForceUpdateClock(position_ms);
  }

  {
    std::promise<void> *pr;
    event->GetDataCopy("pr", pr);
    pr->set_value();
  }
}

void Renderer::RenderVideo(const sp<Event> &event) {
  sp<AVFrame> frame;
  event->GetDataCopy("frame", frame);
  if (renderer_) {
    renderer_(frame);
  }

  std::promise<void> *pr;
  event->GetDataCopy("pr", pr);
  pr->set_value();
}

void Renderer::ClearResource() {
  img_buffer_.clear();
  pcm_buffer_.clear();
}

void Renderer::UpdateSerialNum() {
  ++serial_num_;
  notify_->SetData("serial_num", serial_num_);
}

sp<AVFrame> Renderer::PlayAudio() {
  sp<AVFrame> pcm;
  auto ret = pcm_buffer_.try_pop(pcm);
  Ignore(ret);
  if (pcm) {
    sp<AVFrame> transform_out = pcm;
    if (audio_transform_) audio_transform_->Transform(pcm, transform_out);
//    LOG(VERBOSE) << "play audio:" << pcm->pts;
    return transform_out;
  }
  return nullptr;
}

sp<Renderer> CreateRenderer(const AVStream* video_stream,
                            const AVStream* audio_stream, 
                            MediaType media_type, 
                            const sp<Event>& notify) {
  sp<Renderer> render(new Renderer(video_stream, audio_stream, media_type, notify));
  return render;
}

}
