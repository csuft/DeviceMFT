cmake_minimum_required(VERSION 2.8)
project(insplayer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Werror")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/modules)

########find FFmpeg##############
set(ENV{FFMPEG_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/lib/ffmpeg3)
Find_package(FFMPEG REQUIRED)
message("FFMPEG INCLUDE DIR:" ${FFMPEG_INCLUDE_DIR})
message("FFMPEG LIBRARIES DIR:" ${FFMPEG_LIBRARIES})
include_directories(${FFMPEG_INCLUDE_DIR})
link_directories(${CMAKE_SOURCE_DIR}/lib/ffmpeg3/lib)
link_libraries(${FFMPEG_LIBRARIES})

########find X264###############
link_libraries(x264)


find_package(PkgConfig)
find_package(OpenAL)
find_package(SDL2)
find_package(SDL2_ttf)

pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(LIBEVENT_PTHREAD REQUIRED libevent_pthreads)

include_directories(src)
include_directories(${SDL2_INCLUDE_DIR} ${SDL2_TTF_INCLUDE_DIR})
include_directories(${OpenAL_INCLUDE_DIR})
include_directories(test/gui)


#SDL必须第一个link,否则在linux上会和ffmpeg链接,会导致SDL的pollEvent一直为0
link_libraries(${SDL2_LIBRARY} ${SDL2_TTF_LIBRARIES})
link_directories(${LIBEVENT_LIBRARY_DIRS})
link_libraries(${LIBEVENT_LIBRARIES})
link_libraries(${LIBEVENT_PTHREAD_LIBRARIES})

link_libraries(${OPENAL_LIBRARY})
link_libraries(pthread faad)

##########event loop######
set(eventloop_src
        src/eventloop/event.cpp
        src/eventloop/event_dispatcher.cpp
        )

#########llog#############
set(llog_src
        src/llog/llog.cpp
        src/llog/llog_message.cpp
        )

########av toolbox############
set(av_toolbox_src
        src/av_toolbox/demuxer.cpp
        src/av_toolbox/decoder.cpp
        src/av_toolbox/encoder.cpp
        src/av_toolbox/ffmpeg_util.cpp
        src/av_toolbox/codec_util.cpp
        src/av_toolbox/packet_util.cpp
        src/av_toolbox/frame_util.cpp
        src/av_toolbox/filter_util.cpp
        src/av_toolbox/stream_util.cpp
        src/av_toolbox/bsf_filter.cpp
        src/av_toolbox/muxer.cpp
        src/av_toolbox/scaler.cpp
        src/av_toolbox/video_parser.cpp
        src/av_toolbox/swr_resampler.cpp
        src/av_toolbox/swr_util.cpp
        src/av_toolbox/dynamic_bitset.cpp
        )

###########################insplayer###########################
set(player_src
        src/player/packet_source.cpp
        src/player/url_parser.cpp
        src/player/audio_decode_handler.cpp
        src/player/video_decode_handler.cpp
        src/player/player.cpp
        src/player/audioplay/audio_player.cpp
        src/player/audioplay/openal_audio_player.cpp
        src/player/audioplay/audio_queue_player.cpp
        src/player/renderer.cpp
        src/player/circular_packet_buffer.cpp
        src/player/list_packet_buffer.cpp
        src/player/audio_transform/audio_resample_transform.cpp
        )

#SET(MAC_PLATFORM_SPECIFIC_LIBRARIES
#        "-framework AudioToolbox"
#        "-framework Cocoa"
#        "-framework QuartzCore"
#        "-framework CoreMedia"
#        "-framework VideoToolBox"
#        "-lobjc")
#
add_executable(player_test
        ${player_src}
        ${llog_src}
        ${eventloop_src}
        ${av_toolbox_src}
        )
add_executable(insplayer
        ${player_src}
        ${llog_src}
        ${eventloop_src}
        ${av_toolbox_src}
        render/main.cpp
        )
target_link_libraries(player_test gtest gtest_main)

############################inseditor###################################
set(editor_src
        src/editor/file_media_src.cpp
        src/editor/filter/encode_filter.cpp
        src/editor/filter/decode_filter.cpp
        src/editor/filter/mux_sink.cpp
        src/editor/filter/scale_filter.h
        src/editor/filter/swr_resample_filter.cpp
        src/editor/media_pipe.cpp
        src/editor/merge_media_src.cpp
        )

add_executable(editor_test
        ${editor_src}
        ${llog_src}
        ${av_toolbox_src}
        ${eventloop_src}
        test/editor_test.cpp
        )
target_link_libraries(editor_test gtest gtest_main)

add_executable(all_test
        ${editor_src}
        ${llog_src}
        ${av_toolbox_src}
        ${eventloop_src}
        test/decode_test.cpp
        test/demux_test.cpp
        test/editor_test.cpp
        test/dynamic_circular_buffer_test.cpp
#        test/realtime_src_test.cpp
        test/any_test.cpp
        test/scale_test.cpp
        test/swresample.cpp
        test/transcode.cpp
        test/video_parser_test.cpp
        test/event_test.cpp
        )
target_link_libraries(all_test gtest gtest_main)
