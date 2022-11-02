//
//  publisher.hpp
//  MacVideoPlayer
//
//  Created by steven on 2022/10/24.
//

#ifndef publisher_hpp
#define publisher_hpp
#include <librtmp/rtmp.h>

extern "C" {
    #include "libavcodec/codec.h"
    #include "libavcodec/packet.h"
}

#include <stdio.h>

class Publisher {
    
public:
    void publish();
    RTMP *connect(char *url);
    void sendPacket(AVPacket *avPacket);
    void disConnect();
    void readData(char *filePath);
    static bool isBigEnd();
    static int32_t swapBit(int32_t p);

private:
    RTMP *rtmp = NULL;
    RTMPPacket* packet = NULL;
    
};


#endif /* publisher_hpp */


