//
//  test.hpp
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#ifndef test_hpp
#define test_hpp

#include <stdio.h>

extern "C" {
    #include "libavutil/frame.h"
    #include "libavcodec/packet.h"
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
}

enum RecordType {
    VIDEO,
    AUDIO,
    ALL,
};

class Test{
public:
    void startRecord(RecordType type);
    void stopRecord();
    void convertPcm2AAC();
    void openFile(const char* url);

    
private:
    bool isStop = false;
    void recordAudioTask();
    void recordVideoTask();
    
    /**
     采用h264编码AVFrame为AVPacket，并保存到outFile文件中
     @param codecContext 编码器上下文
     @param frame 编码前的frame数据
     @param avPacket 编码后packet数据
     @param outFile 保存的文件
     */
    void encodeFrame(AVCodecContext *codecContext, AVFrame *frame, AVPacket *avPacket, FILE *outFile);
    
    /**
     将格式为yuv420p的avframe保存到文件中
     @param avFrame 保存后的frame数据
     @param outFile 输出的文件
     */
    void saveFrameYuv420p(AVFrame *avFrame, FILE *outFile);
    
    /**
     创建一个新的AVFrame
     @param width 对应宽度
     @param height
     @return 返回一个AVFrame指针
     */
    AVFrame* createFrame(int width, int height);
    
    /**
     打开编码器
     @param codecContext 传入的AVCodecContext上下文，打开编码器后赋值给其
     @param width 输出宽度
     @param height 输出高度
     */
    void openEncoder(AVCodecContext **codecContext, int width, int height);
    
    /**
     打开video/audio的输入设备
    @return 返回一个设备上下文
     */
    AVFormatContext* openDevice();
};


#endif /* test_hpp */
