//
//  audio_recorder.c
//  MacVideoPlayer
//
//  Created by steven on 2022/9/9.
//

#include "audio_recorder.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/version.h"
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"


void audioRecord(const char *url) {
    AVFormatContext *avCtx = avformat_alloc_context();
    // 打开文件
    avformat_open_input(&avCtx, url, NULL, NULL);
    // 打印输出视频信息
    av_dump_format(avCtx, 0, url, 0);
}


void hello(){
    printf("hello world");
//    swiftHello();
}
