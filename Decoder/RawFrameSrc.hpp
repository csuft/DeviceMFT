//
//  RawFrameSrc.hpp
//  Created by zhangzhongke on 3/13/17.
//  Copyright © 2017 Insta360. All rights reserved.
//

#ifndef RawFrameSrc_hpp
#define RawFrameSrc_hpp

#include <string>
#include <tuple>
#include <av_toolbox/sp.h>
#include <editor/media_src.h>

struct AVStream;

namespace ins {
    
    class any;
    
    class RawFrameSrc : public MediaSrc {
    public:
        
        explicit RawFrameSrc();
        ~RawFrameSrc();
        RawFrameSrc(const RawFrameSrc&) = delete;
        RawFrameSrc(RawFrameSrc &&) = delete;
        RawFrameSrc& operator=(const RawFrameSrc&) = delete;
        
        
        void SetOpt(const std::string &key, const any &val);
        bool Prepare() override;
        void Start() override;
        void Cancel() override;
        void Close() override;
         
    private:
        void OnEnd();
        void OnError();
        void Loop();
        sp<ARVPacket> NewMJpegPacket(const uint8_t * data, int size, bool keyframe, int stream_index);
        
    private:
        int mStreamIndex;
        int mWidth;
        int mHeight;
    };
    
}

#endif /* RawFrameSrc_hpp */
