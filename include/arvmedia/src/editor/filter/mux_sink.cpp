//
// Created by jerett on 16/7/14.
//

#include "mux_sink.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include <av_toolbox/ffmpeg_util.h> 
#include <llog/llog.h>

using namespace std::chrono;

namespace ins {

MuxSink::MuxSink(const std::string &format, const std::string &file_url) : file_url_(file_url) {
  auto out_fmt = NewOutputAVFormatContext(format, file_url);
  muxer_.reset(new Muxer(out_fmt));
}

bool MuxSink::ConfigVideoStream(const sp<AVStream> &vst) {
  if (!spherical_type_.empty() && !stereo_type_.empty()) {
    av_dict_set(&vst->metadata, "spherical_mapping", spherical_type_.c_str(), 0);
    av_dict_set(&vst->metadata, "stereo_3d", stereo_type_.c_str(), 0);

    if (fragment_mp4_) {
      auto ctx = muxer_->fmt_ctx();
      ctx->oformat->flags |= AVFMT_GLOBALHEADER;
      ctx->oformat->flags |= AVFMT_ALLOW_FLUSH;
      av_opt_set(ctx->priv_data, "movflags", "frag_custom+empty_moov", 0);
    }
  }
  return true;
}

bool MuxSink::ConfigAudioStream(const sp<AVStream> &ast) {
  if (spatial_audio_) {
    av_dict_set(&ast->metadata, "spatial_audio", "1", 0);
  }
  return true;
}


bool MuxSink::Init(MediaContext *ctx) {
  sp<AVStream> stream;
  if (ctx->input_stream.stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    stream = muxer_->GetVideoStream();
    if (!ConfigVideoStream(stream)) return false;
    src_video_timebase_ = ctx->input_stream.stream->time_base;
  } else if (ctx->input_stream.stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
    stream = muxer_->GetAudioStream();
    if (!ConfigAudioStream(stream)) return false;
    src_audio_timebase_ = ctx->input_stream.stream->time_base;
  } else {
    LOG(WARNING) << "codec type unknown:" << ctx->input_stream.stream->codecpar->codec_type;
    return false;
  }
  avcodec_parameters_copy(stream->codecpar, ctx->output_stream.codecpar.get());
  stream->codecpar->codec_tag = 0;
  return true;
}

bool MuxSink::Open() {
  if (!is_open_) {
    is_open_ = true;
    return muxer_->Open() == 0;
  }
  return false;
}

bool MuxSink::Filter(MediaContext* ctx, const sp<ARVPacket> &data) {
  auto open_failed = !is_open_ && !Open();
  if (open_failed) return false;
  if (data->media_type == AVMEDIA_TYPE_VIDEO) {
    {
      //handle fragment if set
      if (fragment_mp4_ && ++fragment_counter_ % frames_per_fragment_ == 0) {
        auto fmt_ctx = muxer_->fmt_ctx().get();
        av_write_frame(fmt_ctx, nullptr);
        fragment_counter_ = 0;
        LOG(VERBOSE) << "new fragment.";
      }
    }
    return muxer_->WriteVideoPacket(data, src_video_timebase_) == 0;
  } else if (data->media_type == AVMEDIA_TYPE_AUDIO) {
    auto out_audio_stream = muxer_->GetAudioStream();
    return muxer_->WriteAudioPacket(data, src_audio_timebase_) == 0;
  } else {
    LOG(WARNING) << "mux_sink: unknown type";
  }
  return true;
}

void MuxSink::Close(MediaContext* ctx) {
  muxer_->Close();
}

void MuxSink::Interrupt() {
  muxer_->Interrupt();
}

}
