//
// Created by jerett on 16/7/14.
//

#ifndef INSPLAYER_MEDIA_FILTER_H
#define INSPLAYER_MEDIA_FILTER_H

#include <string>
#include <memory>
#include <av_toolbox/sp.h>
#include <av_toolbox/any.h>

struct AVStream;
struct AVCodecParameters;
struct AVPacket;
struct AVFrame;
struct AVBufferRef;

namespace ins {

struct ARVPacket;

struct MediaContext {
  //read only, pointer will invalid when media src destruct
  struct InputStream {
    int index = -1;
    const AVStream *stream = nullptr;
    AVBufferRef *hw_frame_ctx = nullptr;
  } input_stream;

  struct OutputStream {
    sp<AVCodecParameters> codecpar;
    AVBufferRef *hw_frame_ctx = nullptr;
  } output_stream;

  any user_data;

  /**
   * \brief set by arvmedia, flag to use when Close filter.
   */
  bool canceled = false;
  int error_code = -1;
};

enum NotifyType {
  kNotifyEOF = 0,
  kNotifyFirstVideoPktDts,
  kNotifyFirstAudioPktDts,
  kNotifyUser,
};

struct MediaNotify {
  uint32_t type;
  any user_data;
};

template <typename InDataType>
class MediaSource {
public:
  using in_type = InDataType;
  MediaSource() = default;
  virtual ~MediaSource() = default;
  /**
   * \brief Init ctx method, some filter may be called many times.
   *        Filters must test InDataType in order to make the pipe line work propertly.
   * \param ctx Media ctx.
   * \return Init result
   */
  virtual bool Init(MediaContext *ctx) = 0;
  /**
   * \brief Notify internal event.Won't be called from external threads.
   * \param ctx Media context.
   * \param notify Nofity event.
   */
  virtual void Notify(MediaContext *ctx, const MediaNotify &notify) = 0;
  virtual bool Filter(MediaContext *ctx, const InDataType &data) = 0;
  /**
   * \brief Close ctx. Won't be called from external threads.
   * \param ctx Media context
   */
  virtual void Close(MediaContext *ctx) = 0;
  /**
   * \brief Require cancel, most time invoked by external threads.
   *        Filters whose Filter method may stuck the pipeline should implement this method to make the Filter method return, so the piepeline can cancel propertly.
   *        You should not destroy resource here which should be implemented in Close method.
   * \param ctx Media context
   */
  virtual void Cancel(MediaContext *ctx) = 0;
};

template <typename InDataType, typename OutDataType>
class MediaFilter: public MediaSource<InDataType> {
  template <typename DataType> using SpDataFilter = sp<MediaSource<DataType>>;
public:
  virtual ~MediaFilter() = default;
  using out_type = OutDataType;
  virtual void Notify(MediaContext *ctx, const MediaNotify &notify) override {
    return next_filter_->Notify(ctx, notify);
  }

  void Cancel(MediaContext *ctx) override {
    return next_filter_->Cancel(ctx);
  }

  template <typename NextFilterType>
  NextFilterType& set_next_filter(NextFilterType &next_filter) noexcept {
    next_filter_ = next_filter;
    return next_filter;
  }

protected:
  SpDataFilter<OutDataType> next_filter_;
};

template <typename InDataType>
class MediaSink: public MediaSource<InDataType> {
public:
  virtual ~MediaSink() = default;
  virtual void Notify(MediaContext *ctx, const MediaNotify &notify) override { ; }
  void Cancel(MediaContext *ctx) override { ; }
};

using PacketSource = MediaSource<sp<ARVPacket>>;
using FrameSource = MediaSource<sp<AVFrame>>;
using PacketSink = MediaSink<sp<ARVPacket>>;
using AudioFrameSource = MediaSource<sp<AVFrame>>;

template <typename OutDataType>
using StreamFilter = MediaFilter<sp<ARVPacket>, OutDataType>;

template <typename OutDataType>
using VideoImageFilter = MediaFilter<sp<AVFrame>, OutDataType>;

template <typename OutDataType>
using AudioFrameFilter = MediaFilter<sp<AVFrame>, OutDataType>;

}


#endif //INSPLAYER_MEDIA_FILTER_H
