//
//  test.hpp
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#ifndef media_manage_hpp
#define media_manage_hpp

#include <stdio.h>

extern "C" {
    #include "libavutil/frame.h"
    #include "libavcodec/packet.h"
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/audio_fifo.h"
    #include "libswresample/swresample.h"
    #include "libswscale/swscale.h"
}

enum RecordType {
    VIDEO,
    AUDIO,
    ALL,
};

class MediaManager{
public:
    void startRecord(RecordType type);
    void stopRecord();
    void convertPcm2AAC();
    void openFile(const char* url);
    void pushStream();
    
    /// 播放音视频文件
    /// @param url 文件路径或者流媒体地址
    void play(const char *url);

    
private:
    bool isStop = false;
    SwrContext* swrContext;
    SwsContext* swsContext;
    
    AVStream* videoStream;
    AVStream* audioStream;
    
    int audioFrameNum = 0;
    int videoFrameNum = 0;
    
    void recordAudioTask();
    void recordVideoTask();
    

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
    AVFrame* createYUV420Frame(int width, int height);
    
    
    /// 打开解码器
    /// @param fmtContext 输入的format
    /// @param index 音频或视频的index
    /// @return 返回解码器上下文context
    AVCodecContext* openDecoder(AVFormatContext *fmtContext, int index);
    
    /**
     打开video/audio的输入设备
    @return 返回一个设备上下文
     */
    AVFormatContext* openDevice();
    
    
    /**
     采用h264编码AVFrame为AVPacket，并保存到outFile文件中
     @param codecContext 编码器上下文
     @param outFmtContext 输出上下文
     @param frame 编码前的frame数据
     @param avPacket 编码后packet数据
     */
    void encodeToH264(AVCodecContext *codecContext, AVFormatContext* outFmtContext, AVFrame *frame, AVStream* stream);
    
    void saveH264(AVCodecContext *codecContext, AVFormatContext* outFmtContext, AVFrame *frame, AVPacket *avPacket);
    
    /// AAC编码
    /// @param encodeContext 编码器
    /// @param encodeFmt 输出fmt
    /// @param fifo 音频fifo队列
    /// @param inFrame 解码后的原始数据AVFrame
    void encodeToAAC(AVCodecContext* encodeContext, AVFormatContext* encodeFmt, AVAudioFifo* fifo, AVFrame* inFrame, AVStream* stream);
    
    /**
     打开AAC编码器
     */
    AVCodecContext* openAACEncoder();
    
    
    /**
     打开编码器
     @param width 输出宽度
     @param height 输出高度
     @param isLiveStream 是否为实时流
     @return 0 成功，< 0 失败
     */
    AVCodecContext* openH264Encoder(int width, int height, bool isLiveStream);
    
    
    SwrContext* getSwrContext(int64_t out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
                              int64_t  in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate) {
        SwrContext* swr_Context = swr_alloc();
        swr_alloc_set_opts(swr_Context, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout, in_sample_fmt, in_sample_rate, 1, NULL);
        return swr_Context;
    }
    

    
};


#endif /* test_hpp */
