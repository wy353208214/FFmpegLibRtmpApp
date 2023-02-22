//
//  test.hpp
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#ifndef media_manage_hpp
#define media_manage_hpp

#include <stdio.h>
#include "SDL2/SDL_thread.h"
#include "SDL2/SDL.h"
#include "block_recycler_queue.h"


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

struct VideoPicture {
    AVFrame *frame;
    double pts;
};


struct MediaData {
    AVFormatContext *inFmtCtx;
    
    BlockRecyclerQueue<AVPacket*> *videoPktQueue;
    BlockRecyclerQueue<AVPacket*> *audioPktQueue;
    
    BlockRecyclerQueue<AVFrame*> *videoFrameQueue;
    BlockRecyclerQueue<AVFrame*> *audioFrameQueue;
    
    BlockRecyclerQueue<VideoPicture*> *videoPicQueue;
    
    int videoIndex;
    int audioIndex;
    
    AVCodecContext *videoDecodeCtx;
    AVCodecContext *audioDecodeCtx;
    
    SDL_Window *window;
    SDL_Renderer *render;
    SDL_Texture *texture;
    SDL_Rect rect;
    
    bool quit = false;
    SDL_mutex *mutex;
    
    //音频相关参数
    int out_channels;
    AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    SwrContext *swrContext;
    //解码后的音频数据指针
    uint8_t *audio_buffer;
    //解码后的音频数据长度
    int audio_len;
    //当前正在播放的音频位置
    int audio_buffer_index;
    
    uint8_t *audio_buffer1;
    
    //视频帧转换器
    SwsContext *swsContext;
    
    //音频时钟
    double audio_clock;
    //视频时钟
    double video_clock;
    
    double frame_timer;
    double frame_last_pts;
    double frame_last_delay;
    double frame_last_update;
    
    //是否暂停
    bool pause = false;
    
};

template <typename T>
T sum(T &a, T &b) {
    return a + b;
}


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
    //音频缓冲区大小
    static const size_t MAX_AUDIO_FRAME_SIZE = 192000;
    
    bool isStop = false;
    SwrContext* swrContext;
    SwsContext* swsContext;
    
    AVStream* videoStream;
    AVStream* audioStream;
    
    int audioFrameNum = 0;
    int videoFrameNum = 0;
    
    MediaData mediaData;
    
    SDL_mutex *text_mutex;
    
    
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
     @param height 对应高度
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
     @param stream 视频流stream
     */
    void encodeToH264(AVCodecContext *codecContext, AVFormatContext* outFmtContext, AVFrame *frame, AVStream* stream);
    
    
    /// 保存为H264文件
    /// @param codecContext 编码器上下文
    /// @param outFmtContext 输出上下文
    /// @param frame 解码后的frame
    /// @param avPacket 封装后的packet
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
    
    
    /// 更新视频纹理
    /// @param frame 解码后的原始frame数据
    void updateTexture(AVFrame *frame);
    
    
    /// 解封装Packet
    /// @param data 解码的相关信息，封装在mediadata中
    static int demuxePacket(void *data);
    
    
    /// 解码视频
    /// @param data 解码的相关信息，封装在mediadata中
    static int decodeVideoPacket(void *data);
    
    /// 解码音频
    /// @param data 解码的相关信息，封装在mediadata中
    static int decodeAudioPacket(void *data);
    
     
    /// SDL_Audio播放音频回调函数
    /// @param udata 用户数据
    /// @param stream 音频数据
    /// @param len 要播放的音频数据长度
    static void audioCallBack(void *udata, Uint8 *stream, int len);
    
    
    /// 队列清除数据回调函数
    /// @param pkt 待清除的packet
    static void disCardCallBack(AVPacket *pkt);
    
    /// 队列清除数据回调函数
    /// @param frame 待清除的frame
    static void disCardCallBack(AVFrame *frame);
    
    /// 队列清除数据回调函数
    /// @param vp 待清除的VideoPicture
    static void disCardCallBack(VideoPicture *vp);
    
    /// 视频刷新定时器
    /// @param md 媒体相关的结构体
    /// @param delay 延时时间
    void scheduleVideoRefresh(MediaData *md, int delay);
    
    
    /// 定时器回调函数
    /// @param interval 延时时间
    /// @param udata 用户数据
    static Uint32 sdlRefeshCallBack(Uint32 interval, void *udata);
    
    
    void videoRefreshTimer(void* udata);
    

    static int getAudioByteForPerSec(MediaData *md);
    
    /// 获取音频时钟
    /// @param md  媒体信息相关结构体
    double getAudioClock(MediaData *md);
    
    
    /// 同步重新计算当前帧的视频时钟
    /// @param md 媒体信息相关结构体
    /// @param srcFrame 视频帧Frame
    /// @param pts 当前视频pts
    static double synchronize_video(MediaData *md, AVFrame *srcFrame, double pts);
    
};


#endif /* test_hpp */
