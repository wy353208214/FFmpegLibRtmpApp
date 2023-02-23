//
//  test.cpp
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#include "media_manager.hpp"
#include "publisher.hpp"
#include <iostream>
#include <thread>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/version.h"
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
}

using namespace std;

void MediaManager::startRecord(RecordType type) {
    
    cout<<"ffmpeg version："<<avutil_version()<<endl;
    
    thread task;
    switch (type) {
        case VIDEO:
            task = thread(&MediaManager::recordVideoTask, this);
            task.detach();
            break;
        case AUDIO:
            task = thread(&MediaManager::recordAudioTask, this);
            task.detach();
            break;
        case ALL:
            SDL_Thread *thread1 = SDL_CreateThread(demuxePacket, "线程1", (void*)"线程1");
            SDL_Thread *thread2 = SDL_CreateThread(demuxePacket, "线程2", (void*)"线程2");
            SDL_WaitThread(thread1, NULL);
            SDL_WaitThread(thread2, NULL);
            break;
            //        default:
            //            printf("none");
            //            break;
    }
    cout<<"start record is end"<<endl;
}


void MediaManager::stopRecord() {
    isStop = true;
}

void MediaManager::recordAudioTask(){
    int64_t start = av_gettime_relative();
    
    AVFormatContext *avCtx = NULL;
    AVDictionary *option = NULL;
    char *deviceName = ":0";
    AVPacket *avPacket = av_packet_alloc();
    av_log_set_flags(AV_LOG_DEBUG);
    avdevice_register_all();
    
    AVInputFormat *inFormat = av_find_input_format("avfoundation");
    int ret = avformat_open_input(&avCtx, deviceName, inFormat, &option);
    if( ret < 0) {
        printf("can't open audio device %d", ret);
        av_packet_free(&avPacket);
        return;
    }
    
    const char *savePath = "/Users/steven/Desktop/sound.pcm";
    FILE *file = fopen(savePath, "wb+");//a+ 为连续录制，wb+下次会重新录制
    while(!isStop) {
        ret = av_read_frame(avCtx, avPacket);
        if(ret == -35) {
            continue;
        }
        if(ret < 0) {
            break;
        }
        
        fwrite(avPacket->data, avPacket->size, 1, file);
        fflush(file);
        av_log(NULL, AV_LOG_INFO, "pkt size is %d(%p) \n", avPacket->size, avPacket->data);
        av_packet_unref(avPacket);
    }
    
    int64_t end = av_gettime_relative();
    av_log(NULL, AV_LOG_WARNING, "start: %lld，end：%lld，dura：%lld \n", start, end, end - start);
    
    avformat_close_input(&avCtx);
    av_packet_free(&avPacket);
    fclose(file);
    
    av_log(NULL, AV_LOG_INFO, "Record PCM End \n");
}


Publisher* publish;

void MediaManager::recordVideoTask(){
    char* h264_url = "/Users/steven/Desktop/out.h264";
    char* yuv_url = "/Users/steven/Desktop/video.yuv";
    
    //    FILE *file = fopen(yuv_url, "wb+");//a+ 为连续录制，wb+下次会重新录制
    
    av_log_set_level(AV_LOG_DEBUG);
    avdevice_register_all();
    
    //编码上下文
    AVCodecContext *h264EncodeCtx = NULL;
    AVFormatContext *inFmt = NULL;
    AVFormatContext *h264OutFmt = NULL;
    //yuv420p转换器
    SwsContext *swsCtx = NULL;
    
    //    int width = 1920;
    //    int height = 1080;
    
    //播放命令：ffplay -pixel_format yuyv422 -video_size 1920x1080 -framerate 30  video.yuv
    // 这里的pixel_format与录制格式一致，比如这里是nv12，则播放时也是nv12，也可以是yuyv422、yuv420p
    //列出设备：ffmpeg -f avfoundation -list_devices  true -i ''
    //查看设备支持格式：ffmpeg -f avfoundation -i "0" out.mpg
    
    inFmt = openDevice();
    if (inFmt == NULL) {
        return;
    }
    
    avformat_find_stream_info(inFmt, NULL);
    int videoIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    AVCodecParameters *codecPar = inFmt->streams[videoIndex]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecPar);
    avcodec_open2(codecContext, codec, NULL);
    av_log(NULL, DEBUG, "Codec pix format is：%s \n", av_get_pix_fmt_name((AVPixelFormat) codecPar->format));
    
    int packet_size = av_image_get_buffer_size((AVPixelFormat) codecPar->format, 1920, 1080, 1);
    av_log(NULL, AV_LOG_INFO, "Record frame size is: %d \n", packet_size);
    
    
    //打开编码器
    h264EncodeCtx = openH264Encoder(codecPar->width, codecPar->height, false);
    if (h264EncodeCtx == NULL) {
        return;
    }
    
    //打开输出avformatContext
    h264OutFmt = avformat_alloc_context();
    const AVOutputFormat *avOutputFormat = av_guess_format(nullptr, h264_url, nullptr);
    h264OutFmt->oformat = (AVOutputFormat*) avOutputFormat;
    AVStream *h264_stream = avformat_new_stream(h264OutFmt, h264EncodeCtx->codec);
    avcodec_parameters_from_context(h264_stream->codecpar, h264EncodeCtx);
    if (avio_open(&h264OutFmt->pb, h264_url, AVIO_FLAG_WRITE) < 0) {
        cout<<"avio open h264 file error"<<endl;
        return;
    }
    if (avformat_write_header(h264OutFmt, nullptr) < 0) {
        cout << "文件头写入失败" <<endl;
        return;
    }
    
    
    //创建转换为yuv420p的的outFrame
    AVFrame *outFrame = createYUV420Frame(codecPar->width, codecPar->height);
    //编码后的packet
    AVPacket *h264Packt = av_packet_alloc();
    
    //从设备读取的packet
    AVPacket *inPacket = av_packet_alloc();
    //packet解码后的frame
    AVFrame *frame = av_frame_alloc();
    int count = 0;
    int numFrame = 0;
    while (isStop || count <= 200) {
        int ret = av_read_frame(inFmt, inPacket);
        if(ret == -35) {
            continue;
        }
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "read frame error", NULL);
            break;
        }
        
        count++;
        
        //这里可以直接保存packet，保存的格式为nv12的yuv
        //        fwrite(avPacket->data, 1, packet_size, file);
        //        fflush(file);
        //        av_log(NULL, AV_LOG_INFO, "pkt size is %d(%p) \n", avPacket->size, avPacket->data);
        
        
        // 编码转换，把nv12转换成yuv420p，然后将frame保存到yuv文件中。
        ret = avcodec_send_packet(codecContext, inPacket);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "send packet error");
            return;
        }
        while ((ret = avcodec_receive_frame(codecContext, frame)) >= 0) {
            //            av_log(NULL, DEBUG, "Out Frame pix_fmt is %s \n", av_get_pix_fmt_name((AVPixelFormat) frame->format));
            if (swsCtx == NULL) {
                swsCtx = sws_getContext(frame->width, frame->height, (AVPixelFormat) frame->format, outFrame->width, outFrame->height, (AVPixelFormat) outFrame->format, SWS_BILINEAR, NULL, NULL, NULL);
            }
            sws_scale(swsCtx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, outFrame->data, outFrame->linesize);
            //这里要设置pts，否则会花屏，先计算每一帧的pts，然后累加即可，也可以直接设置为outframe->pts = frame->pts
            double frame_pts = av_q2d(h264EncodeCtx->time_base);
            //转换成毫秒
            //            int64_t pts = av_rescale_q(numFrame++, h264EncodeCtx->time_base, h264_stream->time_base);
            int64_t pts = frame_pts * 1000 * numFrame++;
            outFrame->pts = pts;
            h264Packt->pts = pts;
            //保存frame为yuv文件
            //            saveFrameYuv420p(outFrame, file);
            //            fflush(file);
            
            encodeToH264(h264EncodeCtx, h264OutFmt ,outFrame, h264_stream);
            av_packet_unref(h264Packt);
        }
        av_packet_unref(inPacket);
    }
    //将缓存区的数据全部输出
    encodeToH264(h264EncodeCtx, h264OutFmt, outFrame, h264_stream);
    
    avformat_close_input(&inFmt);
    avio_close(h264OutFmt->pb);
    av_packet_free(&inPacket);
    av_packet_free(&h264Packt);
    av_frame_free(&frame);
    av_frame_free(&outFrame);
    
    //    fclose(file);
    //    fclose(h264File);
    
    av_log(NULL, AV_LOG_INFO, "Record YUV End \n");
}


// 编码有点问题，还没处理完......
void MediaManager::openFile(const char* url) {
    
    AVFormatContext* avFmtCtx = avformat_alloc_context();
    avformat_network_init();
    int ret = avformat_open_input(&avFmtCtx, url, NULL, NULL);
    if (ret < 0) {
        return;
    }
    
    ret = avformat_find_stream_info(avFmtCtx, NULL);
    if (ret < 0) {
        return;
    }
    
    int videoIndex = av_find_best_stream(avFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoIndex < 0) {
        return;
    }
    
    AVStream *videoStream = avFmtCtx->streams[videoIndex];
    AVCodecParameters* videoParam = videoStream->codecpar;
    AVCodec *videoCodec = avcodec_find_decoder(videoParam->codec_id);
    AVCodecContext *decodeCtx = avcodec_alloc_context3(videoCodec);
    if (avcodec_parameters_to_context(decodeCtx, videoParam) != 0)
    {
        return;
    }
    ret = avcodec_open2(decodeCtx, videoCodec, NULL);
    if (ret != 0)
    {
        return;
    }
    
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    
    AVPacket *outPacket =av_packet_alloc();
    FILE *outFile = fopen("/Users/steven/Desktop/out.h264", "w+");
    
    while(true) {
        ret = av_read_frame(avFmtCtx, packet);
        if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "read packet end \n");
            break;
        }
        if (ret != 0)
            break;
        
        bool isVideo = packet->stream_index == videoIndex;
        //        av_log(NULL, AV_LOG_INFO, "read packet size is %d and pts is %d, index is %s \n", packet->size, (int) packet->pts, isVideo ? "Video": "Audio");
        if(isVideo) {
            ret = avcodec_send_packet(decodeCtx, packet);
            if (ret != 0) {
                av_log(NULL, AV_LOG_ERROR, "send packet error when read file \n");
                break;
            }
            while(avcodec_receive_frame(decodeCtx, frame) == 0) {
                av_log(NULL, AV_LOG_INFO, "read frame pts is %d and frame size is %d \n", (int)frame->pts, frame->pkt_size);
                //                encodeFrame(frame, outPacket);
            }
            av_packet_unref(outPacket);
        }
        av_packet_unref(packet);
    }
    
    avformat_close_input(&avFmtCtx);
    avcodec_close(decodeCtx);
    av_packet_free(&packet);
    
    av_packet_free(&outPacket);
    av_frame_free(&frame);
    fclose(outFile);
    
    
}


void MediaManager::saveFrameYuv420p(AVFrame *avFrame, FILE *outFile){
    uint32_t pitchY = avFrame->linesize[0];
    uint32_t pitchU = avFrame->linesize[1];
    uint32_t pitchV = avFrame->linesize[2];
    
    uint8_t *avY = avFrame->data[0];
    uint8_t *avU = avFrame->data[1];
    uint8_t *avV = avFrame->data[2];
    
    // yuv420，Y：w * h
    for (uint32_t i = 0; i < avFrame->height; i++)
    {
        // 降低亮度需要降低y分量值的一半
        fwrite(avY + i * pitchY, avFrame->width, 1, outFile);
    }
    
    for (uint32_t i = 0; i < avFrame->height / 2; i++)
    {
        // 消除U分量
        // memset(avU + i * pitchU, 128, avFrame->width / 2);
        fwrite(avU + i * pitchU, avFrame->width / 2, 1, outFile);
    }
    
    for (uint32_t i = 0; i < avFrame->height / 2; i++)
    {
        // 消除V分量
        // memset(avV + i * pitchV, 128, avFrame->width / 2);
        fwrite(avV + i * pitchV, avFrame->width / 2, 1, outFile);
    }
    
}


AVFrame* MediaManager::createYUV420Frame(int width, int height) {
    AVFrame *outFrame = av_frame_alloc();
    outFrame->width = width;
    outFrame->height = height;
    outFrame->format = AV_PIX_FMT_YUV420P;
    if(av_frame_get_buffer(outFrame, 32) < 0){
        av_log(NULL, AV_LOG_ERROR, "outFrame alloc failed");
        return NULL;
    }
    //    av_image_alloc(outFrame->data, outFrame->linesize, width, height, AV_PIX_FMT_YUV420P, 32);
    return outFrame;
}


void MediaManager::convertPcm2AAC(){
    //pcm原始数据必须是双通道，否则aac会有问题
    //    char *url = "/Users/steven/Movies/Video/S8.pcm";
    char *url = "/Users/steven/Desktop/pcm.pcm";
    char *aac_url = "/Users/steven/Movies/Video/sound.aac";
    
    AVCodec *codec = avcodec_find_encoder_by_name("libfdk_aac");
    if(codec == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Not found codec");
        return;
    }
    
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if(codecCtx == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Not found AVCodecContext");
        return;
    }
    codecCtx->channels = 2;
    codecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
    codecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    codecCtx->sample_rate = 44100;
    codecCtx->bit_rate = 0;
    codecCtx->profile = FF_PROFILE_AAC_HE_V2;
    if(avcodec_open2(codecCtx, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "oepn audio codec failed");
        return;
    }
    
    // av_send_frame 转 av_recive_packet，pcm转换aac
    AVFrame *frame = av_frame_alloc();
    frame->channels = 2;
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->nb_samples = codecCtx->frame_size;
    frame->sample_rate = 44100;
    
    if(av_frame_get_buffer(frame, 0) < 0) {
        av_log(NULL, AV_LOG_ERROR, "alloc avframe failed");
        return;
    }
    //一帧fram的大小 = 一帧采样次数(nb_samples) * 采样通道(channels) * 1次采样大小(2个字节)
    //    int frame_size = frame->nb_samples * frame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    AVPacket *packet = av_packet_alloc();
    FILE *file = fopen(url, "r");
    FILE *aac_file = fopen(aac_url, "w+");
    while (true) {
        int ret = (int) fread(frame->data[0], 1, frame->linesize[0], file);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Read file from frame failed");
            break;
        }else if(ret == 0) {
            av_log(NULL, AV_LOG_INFO, "Read file end \n");
            break;
        }
        ret = avcodec_send_frame(codecCtx, frame);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Send frame failed");
        }
        ret = avcodec_receive_packet(codecCtx, packet);
        if (ret >= 0) {
            fwrite(packet->data, 1, packet->size, aac_file);
        }
        av_packet_unref(packet);
    }
    
    fclose(file);
    fclose(aac_file);
    av_log(NULL, AV_LOG_INFO, "Convert aac end \n");
}



int64_t cur_pts = 0;
void MediaManager::pushStream() {
    //    char *input_url =  "/Users/steven/Movies/Video/S8.flv";
    char *input_url =  "/Users/steven/Movies/Video/luoxiang.flv";
    //    char *input_url =  "/Users/steven/Desktop/out.flv";
    char* rtmp_url = "rtmp://localhost:1935/hls/test";
    
    avformat_network_init();
    AVFormatContext* inFmt = NULL;
    AVFormatContext* rtmpOutFmt = NULL;
    
    AVCodecContext* audioDecodeCtx; //音频解码器
    AVCodecContext* videoDecodeCtx; //视频解码器
    AVCodecContext* h264EnCodecContext = NULL; //h264编码器
    AVCodecContext* aacEnCodecContext = NULL; //aac编码器
    
    
    //打开输入文件，解码音频
    int ret = avformat_open_input(&inFmt, input_url, NULL, NULL);
    if (ret != 0) {
        cout<<"Open input file error"<<endl;
        return;
    }
    avformat_find_stream_info(inFmt, NULL);
    av_dump_format(inFmt, 0, input_url, 0);
    
    //查找音频信息，并打开音频解码器
    int audioIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    audioDecodeCtx = openDecoder(inFmt, audioIndex);
    //查找视频信息，并打开视频解码器
    int videoIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    videoDecodeCtx = openDecoder(inFmt, videoIndex);
    
    //初始化rtmp输出上下文
    ret = avformat_alloc_output_context2(&rtmpOutFmt, NULL, "flv", rtmp_url);
    if (ret < 0) {
        cout<<"打开rtmp推流失败"<<endl;
        return;
    }
    
    //打开AAC编码器
    aacEnCodecContext = openAACEncoder();
    if (aacEnCodecContext == NULL) {
        return;
    }
    //打开h264编码器
    h264EnCodecContext = openH264Encoder(videoDecodeCtx->width, videoDecodeCtx->height, true);
    if(h264EnCodecContext == NULL)
        return;
    
    //这里设置输出Stream，这里因为要求推流的音频aac\视频h264（服务端需要支持hls\dash协议），当然不要求也可以，可以直接推流
    for (int i = 0; i < inFmt->nb_streams; i++) {
        AVStream* inStream = inFmt->streams[i];
        AVCodecParameters* inCodePar = inStream->codecpar;
        const AVCodec* inCodec = avcodec_find_decoder(inCodePar->codec_id);
        
        switch (inCodePar->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (audioDecodeCtx->codec_id == AV_CODEC_ID_AAC){
                    audioStream = avformat_new_stream(rtmpOutFmt, inCodec);
                    avcodec_parameters_copy(audioStream->codecpar, inCodePar);
                }else {
                    //aac编码流
                    audioStream = avformat_new_stream(rtmpOutFmt, aacEnCodecContext->codec);
                    audioStream->codecpar->codec_tag = 0;
                    audioStream->time_base = aacEnCodecContext->time_base;
                    avcodec_parameters_from_context(audioStream->codecpar, aacEnCodecContext);
                    if (rtmpOutFmt->oformat->flags & AVFMT_GLOBALHEADER)
                        aacEnCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                }
                break;
                
            case AVMEDIA_TYPE_VIDEO:
                if (videoDecodeCtx->codec_id == AV_CODEC_ID_H264){
                    videoStream = avformat_new_stream(rtmpOutFmt, inCodec);
                    avcodec_parameters_copy(videoStream->codecpar, inCodePar);
                }else {
                    // h264编码流
                    videoStream = avformat_new_stream(rtmpOutFmt, h264EnCodecContext->codec);
                    videoStream->codecpar->codec_tag = 0;
                    videoStream->time_base = h264EnCodecContext->time_base;
                    avcodec_parameters_from_context(videoStream->codecpar, h264EnCodecContext);
                    if (rtmpOutFmt->oformat->flags & AVFMT_GLOBALHEADER)
                        h264EnCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                }
                break;
                
            default:
                AVStream* outStream = avformat_new_stream(rtmpOutFmt, inCodec);
                avcodec_parameters_copy(outStream->codecpar, inCodePar);
                break;
        }
    }
    
    
    av_dump_format(rtmpOutFmt, 0, rtmp_url, 1);
    //打开rtmp通道
    ret = avio_open(&(rtmpOutFmt->pb), rtmp_url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        cout<<"打开rtmp server失败"<<endl;
        return;
    }
    //写入rtmp头
    ret = avformat_write_header(rtmpOutFmt, 0);
    if (ret < 0) {
        cout<<"写入rtmp server header失败"<<endl;
        return;
    }
    
    
    //初始化缓冲区AVAudioFifo，由于mp3是1153个采样点，aac是1024个采样点，所以需要用到avaudiofifo缓冲器。
    AVAudioFifo* fifo = av_audio_fifo_alloc(aacEnCodecContext->sample_fmt, aacEnCodecContext->channels, aacEnCodecContext->frame_size);
    //初始化音频重采样转换器
    swrContext = getSwrContext(aacEnCodecContext->channel_layout, aacEnCodecContext->sample_fmt, aacEnCodecContext->sample_rate,
                               audioDecodeCtx->channel_layout, audioDecodeCtx->sample_fmt, audioDecodeCtx->sample_rate);
    swr_init(swrContext);
    AVFrame* swrOutFrame = av_frame_alloc();
    swrOutFrame->channels = aacEnCodecContext->channels;
    swrOutFrame->channel_layout = aacEnCodecContext->channel_layout;
    swrOutFrame->format = aacEnCodecContext->sample_fmt;
    swrOutFrame->nb_samples = aacEnCodecContext->frame_size;
    swrOutFrame->sample_rate = aacEnCodecContext->sample_rate;
    if(av_frame_get_buffer(swrOutFrame, 0) < 0) {
        av_log(NULL, AV_LOG_ERROR, "alloc avframe failed");
        return;
    }
    
    //初始化视频转换器
    swsContext = sws_getContext(videoDecodeCtx->width, videoDecodeCtx->height, videoDecodeCtx->pix_fmt,
                                h264EnCodecContext->width, h264EnCodecContext->height, h264EnCodecContext->pix_fmt,
                                SWS_BILINEAR, NULL, NULL, NULL);
    AVFrame* swsOutFrame = createYUV420Frame(h264EnCodecContext->width, h264EnCodecContext->height);
    
    
    //开始解码读取数据
    AVPacket* inPacket = av_packet_alloc();
    AVFrame* audioFrame = av_frame_alloc();
    AVFrame* videoFrame = av_frame_alloc();
    long long startTime = av_gettime();
    int numFrame = 0;
    
    while (true) {
        ret = av_read_frame(inFmt, inPacket);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            cout<<"fifo least data size is "<<av_audio_fifo_size(fifo)<<endl;
            cout<<"Decode audio packet end"<<endl;
            break;
        }else if (ret < 0) {
            cout<<"Decode audio packet error: ret = "<<ret<<endl;
            break;
        }
        
        // 音频mp3转aac，这里未做优化处理，如果原本就是aac无需转换
        if (inPacket->stream_index == audioIndex) {
            
            if(audioDecodeCtx->codec_id == AV_CODEC_ID_AAC){
                AVRational inTimeBase = inFmt->streams[inPacket->stream_index]->time_base;
                AVRational outTimeBase = rtmpOutFmt->streams[inPacket->stream_index]->time_base;
                inPacket->pts = av_rescale_q_rnd(inPacket->pts, inTimeBase, outTimeBase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
                inPacket->dts = av_rescale_q_rnd(inPacket->dts, inTimeBase, outTimeBase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
                inPacket->duration = av_rescale_q_rnd(inPacket->duration, inTimeBase, outTimeBase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
                inPacket->pos = -1;
                ret = av_interleaved_write_frame(rtmpOutFmt, inPacket);
                
                continue;
            }
            
            ret = avcodec_send_packet(audioDecodeCtx, inPacket);
            while (true) {
                ret = avcodec_receive_frame(audioDecodeCtx, audioFrame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }else if (ret < 0) {
                    cout<<"Decode audio receive frame error: ret = "<<ret<<endl;
                    break;
                }
                //先转换指定格式
                swr_convert_frame(swrContext, swrOutFrame, audioFrame);
                
                swrOutFrame->pts = audioFrame->pts;
                swrOutFrame->pkt_dts = audioFrame->pkt_dts;
                swrOutFrame->pkt_pts = audioFrame->pkt_pts;
                swrOutFrame->pkt_duration = audioFrame->pkt_duration;
                
                //开始将frame编码为aac
                encodeToAAC(aacEnCodecContext, rtmpOutFmt, fifo, swrOutFrame, audioStream);
            }
        }
        
        // 视频flv转h264，这里未做优化处理，如果原本就是h264无需转换
        else if(inPacket->stream_index == videoIndex) {
            
            if (videoDecodeCtx->codec_id == AV_CODEC_ID_H264) {
                AVRational inTimeBase = inFmt->streams[inPacket->stream_index]->time_base;
                AVRational outTimeBase = rtmpOutFmt->streams[inPacket->stream_index]->time_base;
                inPacket->pts = av_rescale_q_rnd(inPacket->pts, inTimeBase, outTimeBase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
                inPacket->dts = av_rescale_q_rnd(inPacket->dts, inTimeBase, outTimeBase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
                inPacket->duration = av_rescale_q_rnd(inPacket->duration, inTimeBase, outTimeBase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
                inPacket->pos = -1;
                
                AVRational tb = videoStream->time_base;
                int64_t dts = av_rescale_q(inPacket->dts, tb, av_get_time_base_q());
                //已经过去的时间
                int64_t now = av_gettime() - startTime;
                if (dts > now) {
                    cout<<"休息会"<<endl;
                    av_usleep((int)(dts - now));
                }
                ret = av_interleaved_write_frame(rtmpOutFmt, inPacket);
                continue;
            }
            
            ret = avcodec_send_packet(videoDecodeCtx, inPacket);
            while (true) {
                ret = avcodec_receive_frame(videoDecodeCtx, videoFrame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }else if (ret < 0) {
                    cout<<"Decode video receive frame error: ret = "<<ret<<endl;
                    break;
                }
                
                AVRational tb = inFmt->streams[inPacket->stream_index]->time_base;
                int64_t dts = av_rescale_q(inPacket->dts, tb, av_get_time_base_q());
                //已经过去的时间
                int64_t now = av_gettime() - startTime;
                if (dts > now) {
                    cout<<"休息会"<<endl;
                    av_usleep((int)(dts - now));
                }
                
                //先转换指定格式
                sws_scale(swsContext, (const uint8_t *const *) videoFrame->data, videoFrame->linesize, 0, videoFrame->height, swsOutFrame->data, swsOutFrame->linesize);
                //这里要设置pts，否则会花屏，先计算每一帧的pts，然后累加即可，也可以直接设置为outframe->pts = frame->pts
                double frame_pts = av_q2d(h264EnCodecContext->time_base);
                int64_t pts = frame_pts * 1000 * numFrame++;
                swsOutFrame->pts = pts;
                encodeToH264(h264EnCodecContext, rtmpOutFmt, swsOutFrame, videoStream);
            }
        }
        av_packet_unref(inPacket);
    }
    
    encodeToAAC(aacEnCodecContext, rtmpOutFmt, fifo, swrOutFrame, audioStream);
    encodeToH264(h264EnCodecContext, rtmpOutFmt, swsOutFrame, videoStream);
    
    av_write_trailer(rtmpOutFmt);
    av_packet_free(&inPacket);
    av_frame_free(&videoFrame);
    av_frame_free(&audioFrame);
    av_frame_free(&swsOutFrame);
    av_frame_free(&swrOutFrame);
    
    avcodec_close(videoDecodeCtx);
    avcodec_close(audioDecodeCtx);
    avcodec_close(h264EnCodecContext);
    avcodec_close(aacEnCodecContext);
    
    avio_close(rtmpOutFmt->pb);
    avformat_free_context(rtmpOutFmt);
    avformat_close_input(&inFmt);
    
    sws_freeContext(swsContext);
    swr_free(&swrContext);
    
    av_audio_fifo_free(fifo);
    avformat_network_deinit();
    
    cout<<"推流结束"<<endl;
    
}

void MediaManager::encodeToAAC(AVCodecContext* encodeContext, AVFormatContext* outFmt, AVAudioFifo* fifo, AVFrame* inFrame, AVStream* stream){
    
    AVFrame *outFrame = av_frame_alloc();
    outFrame->channels = encodeContext->channels;
    outFrame->channel_layout = encodeContext->channel_layout;
    outFrame->format = encodeContext->sample_fmt;
    outFrame->nb_samples = encodeContext->frame_size;
    outFrame->sample_rate = encodeContext->sample_rate;
    if(av_frame_get_buffer(outFrame, 0) < 0) {
        av_log(NULL, AV_LOG_ERROR, "alloc avframe failed");
        return;
    }
    
    //mp3 每个pcm 1152个采样点，所以直接解码得到的fram不能直接塞到编码器编码，需要用AVAudioFifo缓冲器
    int cache_size = av_audio_fifo_size(fifo);
    int ret = av_audio_fifo_realloc(fifo, cache_size + inFrame->nb_samples);
    av_audio_fifo_write(fifo, reinterpret_cast<void **>(inFrame->data), inFrame->nb_samples);
    ret = av_audio_fifo_size(fifo);
    
    AVPacket *outPacket = av_packet_alloc();
    while(true) {
        int count = av_audio_fifo_size(fifo);
        if (count < encodeContext->frame_size) {
            break;
        }
        count = av_audio_fifo_read(fifo, reinterpret_cast<void **>(outFrame->data), encodeContext->frame_size);
        if (count < 0) {
            std::cout << "audiofifo 读取数据失败" << std::endl;
            av_packet_free(&outPacket);
            return;
        }
        cur_pts += outFrame->nb_samples;
        outFrame->pts = cur_pts;
        
        ret = avcodec_send_frame(encodeContext, outFrame);
        while (true) {
            ret = avcodec_receive_packet(encodeContext, outPacket);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                cout << "Audio avcodec_receive_packet end:" << ret << endl;
                break;
            } else if (ret < 0) {
                cout << "Audio avcodec_receive_packet fail:" << ret << endl;
                av_packet_free(&outPacket);
                exit(1);
            } else {
                outPacket->pts = inFrame->pkt_pts;
                outPacket->dts = inFrame->pkt_dts;
                outPacket->duration = inFrame->pkt_duration;
                outPacket->stream_index = stream->index;
                cout<<"Audio write data size = "<<outPacket->size<<", pts = "<<outPacket->pts<<", stream_index = "<<outPacket->stream_index<<endl;
                ret = av_interleaved_write_frame(outFmt, outPacket);
                
                if (ret < 0) {
                    char errbuf[100];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    cout << "Audio av_write_frame fail:" << errbuf << endl;
                    av_packet_free(&outPacket);
                    return;
                } else {
                    cout << "Audio av_write_frame success:" << ret << endl;
                }
            }
        }
    }
    av_frame_free(&outFrame);
    av_packet_free(&outPacket);
}


void MediaManager::encodeToH264(AVCodecContext *codecContext, AVFormatContext* outFmtContext, AVFrame *inFrame, AVStream* stream) {
    if (codecContext == NULL)
        return;
    
    int ret = avcodec_send_frame(codecContext, inFrame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Video Encode send frame error when encode \n");
        exit(1);
    }
    AVPacket *outPacket = av_packet_alloc();
    while (true) {
        ret = avcodec_receive_packet(codecContext, outPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            cout << "Video avcodec_receive_packet end:" << ret << endl;
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Video Error encoding frame\n");
            av_packet_free(&outPacket);
            exit(1);
        }
        //这里设置stream index很关键，否则解码端会有问题
        outPacket->pts = inFrame->pts;
        outPacket->dts = inFrame->pts;
        outPacket->stream_index = stream->index;
        cout<<"Video Encode receive packet size =" << outPacket->size<<", stream_index = "<<outPacket->stream_index<<endl;
        ret = av_interleaved_write_frame(outFmtContext, outPacket);
        //                ret = av_write_frame(outFmtContext, outPacket);
        if (ret < 0) {
            cout << "Video av_write_frame fail:" << ret << endl;
            break;
        } else {
            cout << "Video av_write_frame success:" << ret << endl;
        }
    }
    av_packet_free(&outPacket);
}


AVCodecContext* MediaManager::openAACEncoder() {
    //libfdk_aac
    AVCodec* codec = avcodec_find_encoder_by_name("libfdk_aac");
    AVCodecContext* encodecContext = avcodec_alloc_context3(codec);
    encodecContext->sample_rate = 44100;
    encodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
    encodecContext->channels = av_get_channel_layout_nb_channels(encodecContext->channel_layout);
    encodecContext->profile = FF_PROFILE_AAC_HE_V2;
    encodecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    encodecContext->bit_rate = 0;
    encodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    
    
    //ffmpeg自带的aac
    //    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    //    AVCodecContext* encodecContext = avcodec_alloc_context3(codec);
    //    encodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
    //    encodecContext->channels = av_get_channel_layout_nb_channels(encodecContext->channel_layout);
    //    // 如果解码出来的pcm不是44100的则需要进行重采样，重采样需要主要音频时长不变
    //    encodecContext->sample_rate = 44100;
    //    encodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    //    encodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
    //    encodecContext->profile = FF_PROFILE_AAC_LOW;
    //    //ffmpeg默认的aac是不带adts，而fdk_aac默认带adts，这里我们强制不带
    //    encodecContext->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    
    int ret = avcodec_open2(encodecContext, codec, NULL);
    if (ret < 0) {
        cout<<"open encode error = "<<ret<<endl;
        avcodec_free_context(&encodecContext);
        return NULL;
    }
    return encodecContext;
}


AVCodecContext* MediaManager::openH264Encoder(int width, int height, bool isLiveStream) {
    AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if(codec == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Not found codec \n");
        return NULL;
    }
    
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if(codecContext == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Not found AVCodecContext \n");
        return NULL;
    }
    
    //实时流则需要设置AV_CODEC_FLAG_GLOBAL_HEADER，这样不用每帧数据都设置sps/pps，否则无法播放端无法解码播放
    //本地文件如mp4\h264，则不能设置AV_CODEC_FLAG_GLOBAL_HEADER，这样保证本地文件每帧数据都有sps/pps，因为本地文件可以随意拖动播放
    if (isLiveStream)
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    //分辨率
    codecContext->width = width;
    codecContext->height = height;
    //帧率、码率
    codecContext->framerate = AVRational{25, 1};
    codecContext->time_base = AVRational{1, 25};
    codecContext->bit_rate = 600000;
    //GOP
    codecContext->gop_size = 10;
    codecContext->keyint_min = 5;
    //B帧
    codecContext->max_b_frames = 3;
    codecContext->has_b_frames = 1;
    //参考帧
    codecContext->refs = 3;
    //SPS/PPS
    codecContext->profile = FF_PROFILE_H264_HIGH_444;
    codecContext->level = 50;
    
    //设置编码速度，越慢质量越好
    if (codec->id == AV_CODEC_ID_H264) {
        if (isLiveStream) {
            av_opt_set(codecContext->priv_data, "preset", "superfast", 0);
            av_opt_set(codecContext->priv_data, "tune", "zerolatency", 0);
        }else {
            av_opt_set(codecContext->priv_data, "preset", "slow", 0);
        }
    }
    
    
    if(avcodec_open2(codecContext, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Open x264 encoder error \n");
        return NULL;
    }
    return codecContext;
}


AVCodecContext* MediaManager::openDecoder(AVFormatContext *fmtContext, int index) {
    AVCodecParameters* codePar = fmtContext->streams[index]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codePar->codec_id);
    AVCodecContext* decodeCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(decodeCtx, codePar);
    //打开解码器
    int ret = avcodec_open2(decodeCtx, codec, NULL);
    if (ret < 0) {
        cout<<"Open decoder failed"<<endl;
        return NULL;
    }
    return decodeCtx;
}

AVFormatContext* MediaManager::openDevice(){
    AVFormatContext *fmtContext = avformat_alloc_context();
    AVDictionary *option = NULL;
    //    av_dict_set(&option, "video_size", "1920x1080", 0);
    av_dict_set(&option, "framerate", "30", 0);
    av_dict_set(&option, "pixel_format", "nv12", 0);
    //    av_dict_set(&option, "list_devices", "true", 0);
    
    //播放命令：ffplay -pixel_format yuyv422 -video_size 1920x1080 -framerate 30  video.yuv
    // 这里的pixel_format与录制格式一致，比如这里是nv12，则播放时也是nv12
    //列出设备：ffmpeg -f avfoundation -list_devices  true -i ''
    
    char *deviceName = "2"; //设备索引
    AVInputFormat *inFormat = av_find_input_format("avfoundation");
    int ret = avformat_open_input(&fmtContext, deviceName, inFormat, &option);
    if( ret < 0) {
        printf("can't open video device %d", ret);
        return NULL;
    }
    return fmtContext;
}


void MediaManager::play(const char* url) {
    
    //初始化sdl
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    
    avformat_network_init();
    AVFormatContext *inFmt = avformat_alloc_context();
    int ret = avformat_open_input(&inFmt, url, NULL, NULL);
    if (ret < 0) {
        cout<<"open url failed"<<endl;
        return;
    }
    //当播放网络流时，av_read_frame()可能会无法立即返回；
    //当点击关闭按钮时会一直卡在这里，如果需要立即关闭就要设置回调，回调函数返回1时，该函数就会立即返回，从而快速关闭播放器
    inFmt->interrupt_callback.callback = decode_interrupt_cb;
    inFmt->interrupt_callback.opaque = &mediaData;
    
    avformat_find_stream_info(inFmt, NULL);
    //打印视频信息
    av_dump_format(inFmt, 0, url, 0);
    
    int videoIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    int audioIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    
    AVCodecContext *videoDecodeCtx = openDecoder(inFmt, videoIndex);
    AVCodecContext *audioDecodeCtx = openDecoder(inFmt, audioIndex);
    
    
    //创建SDL窗口
    SDL_Window *window = SDL_CreateWindow(inFmt->url, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          videoDecodeCtx->width, videoDecodeCtx->height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        cout<<"SDL_Window init failed"<<endl;
        return;
    }
    SDL_Renderer *render = SDL_CreateRenderer(window, -1, 0);
    if (render == NULL) {
        cout<<"SDL_Render create failed"<<endl;
        return;
    }
    text_mutex = SDL_CreateMutex();
    SDL_Texture *texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, videoDecodeCtx->width, videoDecodeCtx->height);
    if (texture == NULL) {
        cout<<"SDL_Textture create failed"<<endl;
        return;
    }
    
    
    BlockRecyclerQueue<AVPacket*> *videoPktQueue = new BlockRecyclerQueue<AVPacket*>(100);
    BlockRecyclerQueue<AVPacket*> *audioPktQueue = new BlockRecyclerQueue<AVPacket*>(100);
    BlockRecyclerQueue<AVFrame*> *audioFrameQueue = new BlockRecyclerQueue<AVFrame*>(20);
    BlockRecyclerQueue<VideoPicture*> *videoPicQueue = new BlockRecyclerQueue<VideoPicture*>(20);
    
    mediaData.inFmtCtx = inFmt;
    mediaData.audioIndex = audioIndex;
    mediaData.videoIndex = videoIndex;
    mediaData.audioPktQueue = audioPktQueue;
    mediaData.videoPktQueue = videoPktQueue;
    mediaData.audioFrameQueue = audioFrameQueue;
    mediaData.videoPicQueue = videoPicQueue;
    mediaData.audioDecodeCtx = audioDecodeCtx;
    mediaData.videoDecodeCtx = videoDecodeCtx;
    mediaData.window = window;
    mediaData.render = render;
    mediaData.texture = texture;
    mediaData.quit = false;
    
    
    //启动刷新定时器
    scheduleVideoRefresh(&mediaData, 40);
    
    //解封装线程
    SDL_Thread *demuxeThread = SDL_CreateThread(demuxePacket, "解封装线程", (void*) &mediaData);
    //视频解码线程
    SDL_Thread *decodeVideoThread = SDL_CreateThread(decodeVideoPacket, "解码视频线程", (void*) &mediaData);
    //音频解码线程
    SDL_Thread *decodeAudioThread = SDL_CreateThread(decodeAudioPacket, "解码音频线程", (void*) &mediaData);
    
    
    while (true) {
        //监听事件，否则窗口不会创建
        SDL_Event event;
        // WaitEvent作用，事件发生的时候触发，没有事件的时候阻塞在这里。
        SDL_WaitEvent(&event);
        switch (event.type)
        {
            case SDL_WINDOWEVENT:
                // 这里可以监听窗口变化从而更新图像大小，适应窗口变化
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    cout<<"windows size is changed"<<endl;
                }
                break;
            case SDL_QUIT:
                cout<<"SDL quit"<<endl;
                mediaData.quit = true;
                goto _End;
                break;
            case SDL_DISPLAYEVENT:
                videoRefreshTimer(event.user.data1);
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_p:
                        if (mediaData.pause) {
                            mediaData.frame_timer += av_gettime() / 1000000.0 - mediaData.frame_last_update;
                        }
                        mediaData.pause = !mediaData.pause;
                        cout<<(mediaData.pause ? "Stop" : "Start")<<endl;
                        break;
                }
                break;
        }
    }
    
_End:
    audioFrameQueue->notifyWaitGet();
    audioFrameQueue->notifyWaitPut();
    videoPktQueue->notifyWaitGet();
    videoPktQueue->notifyWaitPut();
    audioPktQueue->notifyWaitGet();
    audioPktQueue->notifyWaitPut();
    videoPicQueue->notifyWaitGet();
    videoPicQueue->notifyWaitPut();
    
    SDL_WaitThread(demuxeThread, NULL);
    SDL_WaitThread(decodeVideoThread, NULL);
    SDL_WaitThread(decodeAudioThread, NULL);
    
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(render);
    SDL_CloseAudio();
    SDL_Quit();
    
    avformat_network_deinit();
    avcodec_close(audioDecodeCtx);
    avcodec_close(videoDecodeCtx);
    avformat_close_input(&inFmt);
    
    if (mediaData.swrContext)
        swr_free(&(mediaData.swrContext));
    if(mediaData.audio_buffer)
        av_free(mediaData.audio_buffer);
    if (mediaData.swsContext) {
        sws_freeContext(mediaData.swsContext);
    }
    
    videoPktQueue->discardAll(disCardCallBack);
    audioPktQueue->discardAll(disCardCallBack);
    audioFrameQueue->discardAll(disCardCallBack);
    delete videoPktQueue;
    delete audioPktQueue;
    delete audioFrameQueue;
    delete videoPicQueue;
    
    cout<<"Play is end"<<endl;
}

int MediaManager::demuxePacket(void *data) {
    MediaData *md = (MediaData*) data;
    
    //解封装开始时，获取系统时间，并设置初始延时
    md->frame_timer = (double) av_gettime() / 1000000;
    md->frame_last_delay = 40e-3;
    
    //设置swrContext参数
    md->audio_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE * 4);
    int64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int out_nb_samples = md->audioDecodeCtx->frame_size;
    int out_channels = av_get_channel_layout_nb_channels(out_ch_layout);
    
    md->out_sample_rate = out_sample_rate;
    md->out_channels = out_channels;
    md->out_sample_fmt = out_sample_fmt;
    
    md->swrContext = swr_alloc();
    swr_alloc_set_opts(md->swrContext, out_ch_layout, out_sample_fmt, out_sample_rate, md->audioDecodeCtx->channel_layout, md->audioDecodeCtx->sample_fmt, md->audioDecodeCtx->sample_rate, 0, NULL);
    swr_init(md->swrContext);
    
    //设置SDL音频参数
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = out_nb_samples;
    wanted_spec.callback = audioCallBack;
    wanted_spec.userdata = md;
    if(SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        cout<<"SDL Open Audio failed"<<endl;
    }
    SDL_PauseAudio(0);
    
    //解封装packet
    while (true) {
        
        //结束退出
        if (md->quit)
            break;
        
        //重复利用packet
        AVPacket *pkt = md->videoPktQueue->getUsed();
        if(pkt == NULL)
            pkt = av_packet_alloc();
        int ret = av_read_frame(md->inFmtCtx, pkt);
        if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "read packet end \n");
            break;
        }
        if (ret < 0)
            break;
        
        if (pkt->stream_index == md->videoIndex) {
            md->videoPktQueue->put(pkt);
        }else if(pkt->stream_index == md->audioIndex) {
            md->audioPktQueue->put(pkt);
        }
        
    }
    cout<<"demuxePacket thread end"<<endl;
    return 1;
}

int MediaManager::decodeVideoPacket(void *data) {
    MediaData *md = (MediaData*) data;
    md->swsContext = sws_getContext(md->videoDecodeCtx->width, md->videoDecodeCtx->height, md->videoDecodeCtx->pix_fmt, md->videoDecodeCtx->width,
                                md->videoDecodeCtx->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    double pts;
    
    while (true) {
        
        //结束退出
        if (md->quit)
            break;
        
        AVPacket *pkt = md->videoPktQueue->get();
        pts = 0;
        int ret = avcodec_send_packet(md->videoDecodeCtx, pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            char errbuf[100];
            av_strerror(ret, errbuf, sizeof(errbuf));
            cout << "avcode send packet fail:" << errbuf << endl;
            continue;
        }
        while (true) {
            
            //结束退出
            if (md->quit)
                break;
            
            VideoPicture *vp = md->videoPicQueue->getUsed();
            if (vp == NULL) {
                vp = new VideoPicture;
                vp->frame = av_frame_alloc();
            }
            AVFrame *frame = vp->frame;
            
            ret = avcodec_receive_frame(md->videoDecodeCtx, frame);
            if (ret == AVERROR_EOF || ret == -35) {
                md->videoPicQueue->putToUsed(vp);
                break;
            }
            if (ret < 0) {
                md->videoPicQueue->putToUsed(vp);
                char errbuf[100];
                av_strerror(ret, errbuf, sizeof(errbuf));
                cout << "avcode receive frame fail:" << errbuf << endl;
                break;
            }
            
            //重新计算pts
            int64_t frame_pts = frame->best_effort_timestamp;
            if (frame_pts == AV_NOPTS_VALUE) {
                frame_pts = 0;
            }
            pts = av_q2d(md->inFmtCtx->streams[md->videoIndex]->time_base) * frame_pts;
            pts = synchronize_video(md, frame, pts);
            vp->pts = pts;
            //放入解码后的vp队列中，给播放视频线程使用
            md->videoPicQueue->put(vp);
        }
        //释放pkt，并放入重复队列使用
        av_packet_unref(pkt);
        md->videoPktQueue->putToUsed(pkt);
    }
    
    cout<<"decodeVideoPacket end"<<endl;
    return 1;
}


int MediaManager::decodeAudioPacket(void *data) {
    MediaData *md = (MediaData*) data;
    while (true) {
        //结束退出
        if (md->quit)
            break;
        
        AVPacket *pkt = md->audioPktQueue->get();
        int ret = avcodec_send_packet(md->audioDecodeCtx, pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            char errbuf[100];
            av_strerror(ret, errbuf, sizeof(errbuf));
            cout << "avcode send packet fail:" << errbuf << endl;
            continue;
        }
        
        while (true) {
            
            //结束退出
            if (md->quit)
                break;
            
            AVFrame *frame = md->audioFrameQueue->getUsed();
            if (frame == NULL) {
                frame = av_frame_alloc();
            }
            
            ret = avcodec_receive_frame(md->audioDecodeCtx, frame);
            if (ret == AVERROR_EOF || ret == -35) {
                md->audioFrameQueue->putToUsed(frame);
                break;
            }
            if (ret < 0) {
                md->audioFrameQueue->putToUsed(frame);
                char errbuf[100];
                av_strerror(ret, errbuf, sizeof(errbuf));
                cout << "avcode receive frame fail:" << errbuf << endl;
                break;
            }
            md->audioFrameQueue->put(frame);
        }
        av_packet_unref(pkt);
        md->videoPktQueue->putToUsed(pkt);
        
    }
    cout<<"decodeAudioPacket end"<<endl;
    return 1;
}

int resize = 1;

void MediaManager::updateTexture(AVFrame *frame) {
    //要转换的视频
    int width = mediaData.videoDecodeCtx->width;
    int height =  mediaData.videoDecodeCtx->height;
    
    //TODO
    //这里可以做优化，如果pix_fmt格式一样，就无需转换
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
    uint8_t *video_out_buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    AVFrame *convertFrame = av_frame_alloc();
    av_image_fill_arrays(convertFrame->data, convertFrame->linesize, video_out_buffer, AV_PIX_FMT_YUV420P,
                         width, height, 1);
    sws_scale(mediaData.swsContext, (const uint8_t *const *)frame->data, frame->linesize, 0, height, convertFrame->data, convertFrame->linesize);
    

    // SDL渲染显示视频
    SDL_UpdateYUVTexture(mediaData.texture, NULL,
                         convertFrame->data[0], convertFrame->linesize[0],
                         convertFrame->data[1], convertFrame->linesize[1],
                         convertFrame->data[2], convertFrame->linesize[2]);

    // 设置渲染的位置（0，0）代表window的左上角，以下方式计算可以保证居中显示
    //    rect.x = (win_width - video_width) / 2;
    //    rect.y = (win_height - video_height) / 2;
    //    rect.w = video_width;
    //    rect.h = video_height;

    mediaData.rect.x = 0;
    mediaData.rect.y = 0;
    mediaData.rect.w = width;
    mediaData.rect.h = height;
    //显示图片
    SDL_LockMutex(text_mutex);
    SDL_RenderClear(mediaData.render);
    SDL_RenderCopy(mediaData.render, mediaData.texture, NULL, &(mediaData.rect));
    // SDL_RenderCopyEx(renderer, texture, NULL, NULL, 90, NULL, SDL_FLIP_VERTICAL);
    SDL_RenderPresent(mediaData.render);
    SDL_UnlockMutex(text_mutex);
    
    //释放资源
    av_free(video_out_buffer);
    av_frame_free(&convertFrame);
}


void MediaManager::audioCallBack(void *udata, Uint8 *stream, int len) {
    MediaData *md = (MediaData*) udata;
    
    if (md->quit) {
        return;
    }
    
    //读取数据
    while (len > 0) {

        //由于len可能小于音频缓冲区的的audio_buffer大小，所以需要SDL回调多次才能播放完缓冲区的数据
        //因此要做判断，当缓冲区还有数据的时候不要解码下一帧数据，继续播放上一帧未播放完毕的剩余数据
        if(md->audio_buffer_index >= md->audio_len) {
            
            //暂停情况继续播放静音，但不更新audio_clock
            if(md->pause){
                int one_sample_size = av_samples_get_buffer_size(NULL, md->out_channels, 1, md->out_sample_fmt, 1);
                md->audio_buffer1 = NULL;
                md->audio_len = 512 /one_sample_size * one_sample_size;
            } else {
                AVFrame *frame = md->audioFrameQueue->get();
                if(frame == NULL)
                    return;
                
                //更新下audio_clock
                if (frame->pts != AV_NOPTS_VALUE) {
                    md->audio_clock = av_q2d(md->inFmtCtx->streams[md->audioIndex]->time_base) * frame->pts;
                }
                
                int count = swr_convert(md->swrContext, &(md->audio_buffer), MAX_AUDIO_FRAME_SIZE, (const uint8_t **) frame->data, frame->nb_samples);
                //计算转换后的audio_buffer缓冲区大小
                int buffer_size = count * md->out_channels * av_get_bytes_per_sample(md->out_sample_fmt);
                md->audio_len = buffer_size;
                md->audio_buffer1 = md->audio_buffer;
                
                //重新计算播放当前帧的播放时长，然后再更新audio_clock
                md->audio_clock += (double) buffer_size / (double) getAudioByteForPerSec(md);
                
                //释放frame
                av_frame_unref(frame);
                md->audioFrameQueue->putToUsed(frame);
            }
            //重置index
            md->audio_buffer_index = 0;
        }
        
        //计算当前剩余待播放的音频大小
        int audio_play_len = md->audio_len - md->audio_buffer_index;
        //比较sdl的缓冲区len与audio_play_len的大小，选择小的播放，因为sdl缓冲区最多只能播放len数据
        int temp_len = len > audio_play_len ? audio_play_len : len;
        //SDL开始混音播放数据
//        memcpy(stream, md->audio_buffer + md->audio_buffer_index, temp_len);
        SDL_memset(stream, 0, len);
        if(md->audio_buffer1) {
            SDL_MixAudio(stream, md->audio_buffer1 + md->audio_buffer_index, temp_len, SDL_MIX_MAXVOLUME);
        }
        //更新音频解码缓冲区已播放的位置
        md->audio_buffer_index += temp_len;

        stream += temp_len;
        len -= temp_len;
    }
}


void MediaManager::scheduleVideoRefresh(MediaData *md, int delay) {
    SDL_AddTimer(delay, sdlRefeshCallBack, md);
}


Uint32 MediaManager::sdlRefeshCallBack(Uint32 interval, void *udata) {
    //通知刷新页面，并将数据传递到SDL事件中
    SDL_Event event;
    event.type = SDL_DISPLAYEVENT;
    event.user.data1 = udata;
    SDL_PushEvent(&event);
    return 0;
}


//从videoFrame队列中取出frame
void MediaManager::videoRefreshTimer(void *udata) {
    MediaData *md = (MediaData*) udata;
    if(md->quit){
        return;
    }
    
    if (md->videoPicQueue->getSize() == 0 || md->pause) {
        scheduleVideoRefresh(md, 1);
        return;
    }

    VideoPicture *vp = md->videoPicQueue->get();
    AVFrame *frame = vp->frame;

    //这里是音视频同步的核心算法
    double actual_delay, delay, sync_threshold, ref_clock, diff;
    delay = vp->pts - md->frame_last_pts;
    if (delay <= 0 || delay >= 1.0) {
        delay = md->frame_last_delay;
    }
    md->frame_last_pts = vp->pts;
    md->frame_last_delay = delay;

    ref_clock = getAudioClock(md);
    diff = vp->pts - ref_clock;

    //0.01是10毫秒
    sync_threshold = (delay > 0.01) ? delay : 0.01;
    if (fabs(diff) < 10.0) {
        //如果视频 - 音频 差值小于阈值，说明视频播慢了，需要加快，延时设为0
        if (diff <= -sync_threshold) {
            delay = 0;
        }
        //如果视频 - 音频 差值大于阈值，说明视频播快了，需要放慢，延时加倍
        else if(diff >= sync_threshold) {
            delay = delay * 2;
        }
    }
    md->frame_timer += delay;
    actual_delay = md->frame_timer - (av_gettime() / 1000000.0);

    //延时不能低于10毫秒
    if (actual_delay < 0.010) {
        actual_delay = 0.010;
    }
    
    int dd = (int) (actual_delay * 1000 + 0.5);
    //设置下次刷新时间，比如4000代表毫秒，和刷新率有关，25fps相当于一帧1/25秒
    scheduleVideoRefresh(md, dd);
    
    //更新视频上次刷新时间，用于暂停时同步时间使用
    md->frame_last_update = av_gettime() / 1000000.0;
    
    //播放展示视频帧
    updateTexture(frame);
    
    //用完之后放到回收队列中，可重复用
    av_frame_unref(frame);
    md->videoPicQueue->putToUsed(vp);
}


double MediaManager::getAudioClock(MediaData *md) {
    double pts = md->audio_clock;
    //剩余未播放的音频大小
    int hw_buffer_size = md->audio_len - md->audio_buffer_index;
    //一秒钟播放的音频大小 = 采样位数 * 采样通道数 * 采样率
    int byte_per_sec = 0;
    //判断音频流是否为null
    if(md->inFmtCtx->streams[md->audioIndex]) {
        byte_per_sec = getAudioByteForPerSec(md);
    }
    if (byte_per_sec) {
        //计算剩余数据的pts
        pts -= (double) hw_buffer_size / byte_per_sec;
    }
    return pts;
}


double MediaManager::synchronize_video(MediaData *md, AVFrame *srcFrame, double pts) {
    double frame_delay;
    if(pts != 0) {
        md->video_clock = pts;
    }else {
        pts = md->video_clock;
    }
    //计算一帧的时长
    frame_delay = av_q2d(md->inFmtCtx->streams[md->videoIndex]->time_base);
    //如果有重复图片，需要重新计算，这里主要是为了防止frame没有pts时，重新估算video pts
    frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);
    md->video_clock += frame_delay;
    return pts;
}


void MediaManager::disCardCallBack(AVPacket *pkt) {
    if (pkt != NULL) {
        av_packet_free(&pkt);
    }
}


void MediaManager::disCardCallBack(AVFrame *frame) {
    if (frame != NULL)
        av_frame_free(&frame);
}

void MediaManager::disCardCallBack(VideoPicture *vp) {
    if (vp != NULL)
        av_frame_free(&(vp->frame));
        delete vp;
}


int MediaManager::getAudioByteForPerSec(MediaData *md) {
    //采样位数 * 采样通道数 * 采样率
    int bytes = md->out_channels * md->out_sample_rate * av_get_bytes_per_sample(md->out_sample_fmt);
    return bytes;
}


