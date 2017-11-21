#pragma once

#include <eventloop/event_handler.h>
#include <av_toolbox/threadsafe_queue.h>
#include <av_toolbox/sp.h>
#include "audio_transform/audio_transform.h"
#include "media_type.h"

struct AVFrame;
struct AVStream;

namespace ins {

class Event;
class EventDispatcher;
class MediaClock;
class AudioPlayer;

/*!
* \breif Renderer module is responsible for video and audio data dipsatching and sync.
*/
//TODO: More test
class Renderer : public EventHandler,
                 public std::enable_shared_from_this<Renderer> {
  friend sp<Renderer> CreateRenderer(const AVStream *video_stream, 
                                     const AVStream *audio_stream,
                                     MediaType media_type, 
                                     const sp<Event> &notify);
public:
  using RenderFunc = std::function<void(const sp<AVFrame>&)>;
  enum {
    kRenderEof = 6000,
  };

public:
  ~Renderer() noexcept;
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  bool Prepare();
  void StartAsync();
  void PauseSync();
  void ResumeSync();
  void RenderVideoSync(const sp<AVFrame> &frame);
  void SeekSync(int64_t position_ms);
  void ReleaseSync();

  bool CheckEvent(const sp<Event> &event) const;

  void SetVideoRenderer(const RenderFunc &renderer) {
    renderer_ = renderer;
  }

  void set_audio_transform(const sp<AudioTransform> &transform) {
    audio_transform_ = transform;
  }

  void EnqueueVideo(const sp<AVFrame> &frame);
  void EnqueueVideoEOF() {
    video_in_eof_ = true;
  }
  void EnqueueAudio(const sp<AVFrame> &frame);
  void EnqueueAudioEOF() {
    audio_in_eof_ = true;
  }
  bool VideoBufferIsFull() const;
  bool AudioBufferIsFull() const;
  int64_t CurrentPosition() const;

private:
  Renderer() = default;
  explicit Renderer(const AVStream *video_stream,
                    const AVStream *audio_stream,
                    MediaType media_type, 
                    const sp<Event> &notify) noexcept;
  void HandleEvent(const sp<Event> &event) override;
  void Start(const sp<Event> &event);
  void Pause(const sp<Event> &event);
  void Resume(const sp<Event> &event);
  void OnLoop(const sp<Event> &event);
  void ResetEOFStatus();
  void Seek(const sp<Event> &event);
  void RenderVideo(const sp<Event> &event);
  void ClearResource();

  void UpdateSerialNum();
  sp<AVFrame> PlayAudio();

private:
  MediaType media_type_ = kFileMedia;
  int64_t next_video_pts_ = 0;
  int64_t serial_num_ = 0;
  bool video_in_eof_ = false;
  bool video_out_eof_ = false;
  bool audio_in_eof_ = false;
  bool audio_out_eof_ = false;
  bool eof_ = false;

  const static int kMaxImgBufferSize = 10;
  const static int kMaxPCMBufferSize = 20;
  ThreadSafeQueue<sp<AVFrame>> img_buffer_;
  ThreadSafeQueue<sp<AVFrame>> pcm_buffer_;

  const AVStream *video_stream_ = nullptr;
  const AVStream *audio_stream_ = nullptr;
  sp<MediaClock> media_clock_;
  sp<AudioPlayer> audio_player_;
  RenderFunc renderer_;
  sp<AudioTransform> audio_transform_;

  sp<Event> notify_;
  sp<EventDispatcher> dispatcher_;
};

/**
 * \brief Renderer factory method
 * \return shared renderer
 */
sp<Renderer> CreateRenderer(const AVStream *video_stream,
                            const AVStream *audio_stream,
                            MediaType media_type,
                            const sp<Event> &notify);

}