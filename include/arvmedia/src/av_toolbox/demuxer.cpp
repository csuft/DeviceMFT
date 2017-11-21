//
// Created by jerett on 16/12/7.
//

#include "demuxer.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
#include <llog/llog.h>
#include "ffmpeg_util.h"
#include "bsf_filter.h"

namespace ins {

Demuxer::Demuxer(const std::string &file) noexcept : file_(file) {
  ;
}

ins::Demuxer::~Demuxer() {
  Close();
}

const AVFormatContext* Demuxer::fmt_ctx() const {
  return fmt_ctx_;
}

const AVStream* Demuxer::video_stream() const {
  if (fmt_ctx_ && video_stream_index_ >= 0) return fmt_ctx_->streams[video_stream_index_];
  return nullptr;
}

const AVStream* Demuxer::audio_stream() const {
  if (fmt_ctx_ && audio_stream_index_ >= 0) return fmt_ctx_->streams[audio_stream_index_];
  return nullptr;
}

const AVStream* Demuxer::StreamAtIndex(unsigned int index) const {
  if (fmt_ctx_ && index < fmt_ctx_->nb_streams) return fmt_ctx_->streams[index];
  return nullptr;
}

int64_t Demuxer::video_duration() const {
  auto video_st = video_stream();
  if (fmt_ctx_ && video_st && video_st->duration >= 0) {
    return TimestampToMs(video_st->duration, video_st->time_base);
  } else {
    return -1;
  }
}

int64_t Demuxer::audio_duration() const {
  auto audio_st = audio_stream();
  if (fmt_ctx_ && audio_st && audio_st->duration >= 0) {
    return TimestampToMs(audio_st->duration, audio_st->time_base);
  } else {
    return -1;
  }
}

int64_t Demuxer::duration() const {
  if (fmt_ctx_) {
    return TimestampToMs(fmt_ctx_->duration, {1, AV_TIME_BASE});
  } else {
    return -1;
  }
}

int Demuxer::InterruptCallback(void* p) {
  auto self = reinterpret_cast<Demuxer*>(p);
  return self->io_interrupt_;
}

int Demuxer::Open(std::map<std::string, std::string> *options) {
  AVDictionary *opt_dic = nullptr;
  if (options) {
    for (auto &opt : *options) {
      av_dict_set(&opt_dic, opt.first.c_str(), opt.second.c_str(), 0);
    }
  }

  fmt_ctx_ = avformat_alloc_context();
  fmt_ctx_->interrupt_callback.callback = InterruptCallback;
  fmt_ctx_->interrupt_callback.opaque = this;
  auto ret = avformat_open_input(&fmt_ctx_, file_.c_str(), nullptr, &opt_dic);
  if (ret == 0) {
    avformat_find_stream_info(fmt_ctx_, nullptr);
    video_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    subtitle_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    av_dump_format(fmt_ctx_, 0, fmt_ctx_->filename, 0);
    //LOG(VERBOSE) << "is seekable:" << fmt_ctx_->pb->seekable;

    //open h264_mp4toannexb filter
    //check wether extradata is mp4 format
    auto vst = const_cast<AVStream*>(video_stream());
    if (vst && vst->codecpar->codec_id == AV_CODEC_ID_H264 && vst->codecpar->extradata[0] == 1) {
      LOG(VERBOSE) << "check is mp4 format, need filter to annexb";
      filter_ = std::make_shared<BSFFilter>();
      ret = filter_->Open("h264_mp4toannexb", vst);
      if (ret != 0) return ret;
      avcodec_parameters_copy(vst->codecpar, filter_->bsf_ctx()->par_out);
      vst->time_base = filter_->bsf_ctx()->time_base_out;
    }
  } else {
    LOG(ERROR) << "avformat open input error:" << ins::FFmpegErrorToString(ret);
  }
  return ret;
}

void Demuxer::Close() {
  if (!closed_) {
    if (fmt_ctx_ != nullptr) {
      avformat_close_input(&fmt_ctx_);
    }
    closed_ = true;
  }
}

int Demuxer::ReadNextPacket(sp<ARVPacket>& pkt) {
  AVPacket tmp_pkt;
  av_init_packet(&tmp_pkt);
  auto ret = av_read_frame(fmt_ctx_, &tmp_pkt);
  if (ret == 0) {
    pkt = NewPacket();
    av_packet_ref(pkt.get(), &tmp_pkt);
    av_packet_unref(&tmp_pkt);
    if (pkt->stream_index == video_stream_index_) {
      pkt->media_type = AVMEDIA_TYPE_VIDEO;
    } else if (pkt->stream_index == audio_stream_index_) {
      pkt->media_type = AVMEDIA_TYPE_AUDIO;
    } else if (pkt->stream_index == subtitle_stream_index_) {
      pkt->media_type = AVMEDIA_TYPE_SUBTITLE;
    }
  } else {
    pkt = nullptr;
  }
  return ret;
}

int Demuxer::Seek(int stream_index, int64_t timestamp, int flags) {
  eof_ = false;
  return av_seek_frame(fmt_ctx_, stream_index, timestamp, flags);
}

int Demuxer::NextPacket(sp<ARVPacket> &pkt) {
  auto ret = ReadNextPacket(pkt);
  //check is audio return directly or filter == nullptr
  if (filter_ == nullptr || (ret == 0 && pkt->media_type != AVMEDIA_TYPE_VIDEO)) return ret;
  //handle eof logic
  if (ret == AVERROR_EOF && !eof_) {
    eof_ = true;
    LOG(VERBOSE) << "ReadPacket eof.";
  } else if (ret == 0 && pkt->media_type == AVMEDIA_TYPE_VIDEO) { //check is video read success
    ret = filter_->SendPacket(pkt);
    if (ret != 0) LOG(WARNING) << "bsf_filter, send pkt err:" << FFmpegErrorToString(ret);
  } else { //return read err directly
    return ret;
  }
  pkt = NewPacket(); //New pkt to Receive
  ret = filter_->ReceivePacket(pkt);
  if (ret == 0) {
    pkt->media_type = AVMEDIA_TYPE_VIDEO;
  } else if (ret == AVERROR_EOF) {
    LOG(VERBOSE) << "filter receive pkt eof";
  } else if (eof_) {
    ret = AVERROR_EOF;
  }
  return ret;
}

}
