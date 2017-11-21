//
//  player.h
//
//  Created by jerett on 16/2/17.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef player_h
#define player_h

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <bitset>
#include <av_toolbox/sp.h>
#include <eventloop/event_handler.h>
#include "audio_transform/audio_transform.h"
#include "media_type.h"

struct AVFrame;
struct AVStream;
struct AVFormatContext;

namespace ins {

class PacketSource;
class VideoDecodeHandler;
class AudioDecodeHandler;
class AudioPlayer;
class Event;
class EventDispatcher;
class MediaClock;
class Renderer;

struct SeekOperation {
  int64_t seek_position = -1;
  bool seek_to_key = true;
};

//TODO:support audio file play
class Player: public EventHandler, public std::enable_shared_from_this<Player> {
  using RenderFunc = std::function<void(const sp<AVFrame>&)>;

public:
  enum NotifyType {
    kNotifyStatus,
    kNotifyBufferingProgress
  };

  enum PlayerStatus {
    kStatusUnKnown = 0,
    kStatusPrepared,
    kStatusPlaying,
    kStatusBuffering,
    kStatusPaused,
    kStatusEnd,
    kStatusFailed,
  };

  using NotifyCallback = std::function<void(NotifyType type, int val, int extra)>;

  explicit Player(const std::string &url);
  Player(const Player&) = delete;
  Player(Player&&) = delete;
  Player& operator=(const Player&) = delete;
  ~Player() noexcept;

  /**
   * notify func must not change player state, or other action which spends long time, which may lead to player thread blocking.
   */
  void SetStatusNotify(const NotifyCallback &notify_func) {
    notifier_ = notify_func;
  }
  // set video render. Method will call on Render time
  // @param render_func Render function
  void SetVideoRenderer(const RenderFunc &render_func);

  sp<AudioTransform> set_audio_transform(const sp<AudioTransform> &transform);
  /**
   * ready to play in Unknown state.When state change to Prepare, you can fetch media info like width or height
   */
  void PrepareAsync();
  /**
   *  start Play
   */
  void PlayAsync();
  /**
   *  pause play
   */
  void PauseAsync();
  // seek to time in ms
  // @param position_ms seeking time
  // @param seek_to_key check whether jump to key only. if false, will lead to accurate seek
  void SeekAsync(int64_t position_ms, bool seek_to_key = true);
  /**
   * destroy player.You need Release before Destruct
   */
  void ReleaseSync();
  /**
   *  fetch current position 
   *  @return position in ms
   */
  int64_t CurrentPosition() const;
  /**
   *  get video width
   *
   *  @return video width
   */
  int GetVideoWidth() const;
  /**
   *  get video height
   *
   *  @return video height
   */
  int GetVideoHeight() const;
  // get video framerate
  // @return framerate in double
  double GetFramerate() const;
  /**
   *  get video duration in ms
   *  @return duration in ms
   */
  int64_t GetDuration() const;
  // set option for player
  // @param key  option key
  // @param val  option value
  void SetOption(const std::string &key, const std::string &val) {
    options_[key] = val;
  }
  /**
   * \brief Check wether has video stream
   * \return If have video stream, return true
   */
  bool HasVideo() const {
    return video_stream_ != nullptr;
  }
  /**
   * \brief Check whther has audio stream
   * \return If have audio stream, return true
   */
  bool HasAudio() const {
    return audio_stream_ != nullptr;
  }

protected:
  void HandleEvent(const sp<ins::Event> &notify) override;

private:
  void HandleSourcePrepared(const sp<Event> &notify);
  void HandleFeedVideoPacket(const sp<Event> &notify);
  void HandleGetDecodedVideo(const sp<Event> &notify);
  void HandleGetDecodedAudio(const sp<Event> &notify);
  void HandleFeedAudioPacket(const sp<Event> &notify);

  void Prepare();
  void Play();
  void Replay();
  void Pause();
  void CheckBuffer();
  void Resume();
  void Seek(int64_t position_ms, bool seek_to_key);
  void SeekVideo(int64_t position_ms, bool seek_to_key);
  void SeekAudio(int64_t position_ms, bool seek_to_key);
  void OnEnd();

  /***********************notify func*************************/
  void NotifyStatus() const;
  void NotifyBufferStatus(int progress) const;


private:
  enum SeekStatus {
    kStatusIdle,
    kStatusToSeek,
  };

  const static int kBufferFlagVideoIndex = 0;
  const static int kBufferFlagAudioIndex = 1;

private:
  MediaType media_type_ = kFileMedia;
  int64_t duration_ = -1;
  int64_t video_duration_ = -1;
  int64_t audio_duration_ = -1;

  std::atomic_int status_ = {kStatusUnKnown};
  SeekStatus seek_status_ = kStatusIdle;

  bool video_source_end_ = false;
  bool audio_source_end_ = false;

//    int64_t seek_position_ms_ = 0;
  SeekOperation last_seek_operation_;
  bool interrupt_paused_seek_ = false;

  std::string url_;
  std::unordered_map<std::string, std::string> options_;

  std::bitset<2> buffer_flags;

  sp<VideoDecodeHandler> video_handler_;
  sp<AudioDecodeHandler> audio_handler_;
  sp<PacketSource> packet_source_;
  sp<Renderer> renderer_;

  const AVStream *video_stream_ = nullptr;
  const AVStream *audio_stream_ = nullptr;

  NotifyCallback notifier_;
  RenderFunc render_func_;
  sp<AudioTransform> audio_transform_;
  sp<EventDispatcher> dispatcher_;
};
}

#endif
