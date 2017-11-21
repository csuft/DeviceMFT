//
// Created by jerett on 16/12/6.
//

#include "muxer.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}
#include "ffmpeg_util.h"
#include <llog/llog.h>

namespace ins {

Muxer::Muxer(const sp<AVFormatContext> &ctx) noexcept : fmt_ctx_(ctx) {
}

Muxer::~Muxer() {
  Close();
}

sp<AVStream>& Muxer::GetVideoStream() {
  if (video_stream_ == nullptr) {
    video_stream_ = AddAVStream(fmt_ctx_);
  }
  return video_stream_;
}

sp<AVStream>& Muxer::GetAudioStream() {
  if (audio_stream_ == nullptr) {
    audio_stream_ = AddAVStream(fmt_ctx_);
  }
  return audio_stream_;
}

int Muxer::InterruptCallback(void* opaque) {
  ins::Muxer *muxer = reinterpret_cast<ins::Muxer*>(opaque);
  return muxer->io_interrupt_;
}

int Muxer::Open() {
  std::lock_guard<std::mutex> lck(mtx_);
  fmt_ctx_->interrupt_callback.callback = InterruptCallback;
  fmt_ctx_->interrupt_callback.opaque = this;
  if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
    auto ret = avio_open2(&fmt_ctx_->pb, fmt_ctx_->filename, AVIO_FLAG_WRITE, &fmt_ctx_->interrupt_callback, nullptr);
    if (ret < 0) {
      LOG(ERROR) << "avio open error:" << FFmpegErrorToString(ret);
      if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        avio_close(fmt_ctx_->pb);
      }
      return ret;
    }
  }

  auto ret = avformat_write_header(fmt_ctx_.get(), nullptr);
  if (ret < 0) {
    LOG(ERROR) << "writer header error:" << FFmpegErrorToString(ret);
    return ret;
  }
  av_dump_format(fmt_ctx_.get(), 0, fmt_ctx_->filename, 1);
  open_ = true;
  return 0;
}

int Muxer::WriteVideoPacket(const sp<AVPacket> &pkt, const AVRational &src_timebase) {
  av_packet_rescale_ts(pkt.get(), src_timebase, video_stream_->time_base);
  pkt->stream_index = video_stream_->index;
  return WritePacket(pkt);
}

int Muxer::WriteAudioPacket(const sp<AVPacket> &pkt, const AVRational &src_timebase) {
  av_packet_rescale_ts(pkt.get(), src_timebase, audio_stream_->time_base);
  pkt->stream_index = audio_stream_->index;
  return WritePacket(pkt);
}

void Muxer::Interrupt() {
  io_interrupt_ = true;
}

int Muxer::WritePacket(const sp<AVPacket> &pkt) {
  std::lock_guard<std::mutex> lck(mtx_);
  auto ret = av_interleaved_write_frame(fmt_ctx_.get(), pkt.get());
  if (ret != 0) {
    LOG(ERROR) << "write frame err:" << FFmpegErrorToString(ret);
  }
  return ret;
}

int Muxer::Close() {
  std::lock_guard<std::mutex> lck(mtx_);
  if (open_) {
    open_ = false;
    auto ret = av_write_trailer(fmt_ctx_.get());
    if (ret != 0) {
      LOG(ERROR) << "write trailer error:" << FFmpegErrorToString(ret);
      return ret;
    }
    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
      avio_close(fmt_ctx_->pb);
    }
    fmt_ctx_.reset();
    LOG(VERBOSE) << "muxer close.";
  }
  return true;
}

}
