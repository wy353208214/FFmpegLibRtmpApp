//
//  test.cpp
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#include "test.hpp"
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
            printf("All");
            break;
        default:
            printf("none");
            break;
    }
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
    
    publish = new Publisher();
    publish->connect("rtmp://localhost:1935/hls/test");
    
    FILE *h264File = fopen("/Users/steven/Desktop/out.h264", "wb+");
    FILE *file = fopen("/Users/steven/Desktop/video.yuv", "wb+");//a+ 为连续录制，wb+下次会重新录制
    
    av_log_set_level(AV_LOG_DEBUG);
    avdevice_register_all();
    
    //编码上下文
    AVCodecContext *encodeCtx = NULL;
    //yuv420p转换器
    SwsContext *swsContext = NULL;
    
    int width = 1920;
    int height = 1080;
    
    //播放命令：ffplay -pixel_format yuyv422 -video_size 1920x1080 -framerate 30  video.yuv
    // 这里的pixel_format与录制格式一致，比如这里是nv12，则播放时也是nv12，也可以是yuyv422、yuv420p
    //列出设备：ffmpeg -f avfoundation -list_devices  true -i ''
    
    AVFormatContext *avCtx = openDevice();
    if (avCtx == NULL) {
        return;
    }
    
    avformat_find_stream_info(avCtx, NULL);
    int videoIndex = av_find_best_stream(avCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    AVCodecParameters *codecPar = avCtx->streams[videoIndex]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecPar);
    avcodec_open2(codecContext, codec, NULL);
    av_log(NULL, DEBUG, "Codec pix format is：%s \n", av_get_pix_fmt_name((AVPixelFormat) codecPar->format));
    
    int packet_size = av_image_get_buffer_size((AVPixelFormat) codecPar->format, 1920, 1080, 1);
    av_log(NULL, AV_LOG_INFO, "Record frame size is: %d \n", packet_size);
    
    //打开编码器
    openH264Encoder(&encodeCtx, width, height);
    //创建转换为yuv420p的的outFrame
    AVFrame *outFrame = createFrame(width, height);
    //编码后的packet
    AVPacket *h264Packt = av_packet_alloc();
    
    //从设备读取的packet
    AVPacket *avPacket = av_packet_alloc();
    //packet解码后的frame
    AVFrame *frame = av_frame_alloc();
    int count = 0;
    int numFrame = 0;
    while (isStop || count <= 200) {
        int ret = av_read_frame(avCtx, avPacket);
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
        ret = avcodec_send_packet(codecContext, avPacket);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "send packet error");
            return;
        }
        while ((ret = avcodec_receive_frame(codecContext, frame)) >= 0) {
            //            av_log(NULL, DEBUG, "Out Frame pix_fmt is %s \n", av_get_pix_fmt_name((AVPixelFormat) frame->format));
            if (swsContext == NULL) {
                swsContext = sws_getContext(frame->width, frame->height, (AVPixelFormat) frame->format, outFrame->width, outFrame->height, (AVPixelFormat) outFrame->format, SWS_BILINEAR, NULL, NULL, NULL);
            }
            sws_scale(swsContext, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, outFrame->data, outFrame->linesize);
            //这里要设置pts，否则会花屏，先计算每一帧的pts，然后累加即可，也可以直接设置为outframe->pts = frame->pts
            double frame_pts = av_q2d(encodeCtx->time_base);
            //转换成毫秒
            
            int64_t pts = frame_pts * 1000 * numFrame++;
            outFrame->pts = pts;
            h264Packt->pts = pts;
            //保存frame为yuv文件
            //            saveFrameYuv420p(outFrame, file);
            //            fflush(file);
            
            encodeToH264(encodeCtx, outFrame, h264Packt, h264File);
            
            fflush(h264File);
            av_packet_unref(h264Packt);
        }
        av_packet_unref(avPacket);
    }
    //将缓存区的数据全部输出
    encodeToH264(encodeCtx, NULL, h264Packt, h264File);
    avformat_close_input(&avCtx);
    av_packet_free(&avPacket);
    av_packet_free(&h264Packt);
    av_frame_free(&frame);
    av_frame_free(&outFrame);
    
    fclose(file);
    fclose(h264File);
    
    publish->disConnect();
    
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


void MediaManager::encodeToH264(AVCodecContext *codecContext, AVFrame *frame, AVPacket *avPacket, FILE *outFile) {
    if (codecContext == NULL)
        return;
    
    int ret = avcodec_send_frame(codecContext, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Encode send frame error when encode \n");
        exit(1);
    }
    
    while (true) {
        ret = avcodec_receive_packet(codecContext, avPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }
        av_log(NULL, AV_LOG_INFO, "Encode receive packet size is %d \n", avPacket->size);
        //        //保存数据到文件中
        //        fwrite(avPacket->data, 1, avPacket->size, outFile);
    }
    
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


AVFrame* MediaManager::createFrame(int width, int height) {
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


AVFormatContext* MediaManager::openDevice(){
    AVFormatContext *fmtContext = NULL;
    AVDictionary *option = NULL;
    av_dict_set(&option, "video_size", "1920x1080", 0);
    av_dict_set(&option, "framerate", "25", 0);
    av_dict_set(&option, "pixel_format", "nv12", 0);
    //播放命令：ffplay -pixel_format yuyv422 -video_size 1920x1080 -framerate 30  video.yuv
    // 这里的pixel_format与录制格式一致，比如这里是nv12，则播放时也是nv12
    //列出设备：ffmpeg -f avfoundation -list_devices  true -i ''
    
    char *deviceName = "2";
    AVInputFormat *inFormat = av_find_input_format("avfoundation");
    int ret = avformat_open_input(&fmtContext, deviceName, inFormat, &option);
    if( ret < 0) {
        printf("can't open video device %d", ret);
        return NULL;
    }
    return fmtContext;
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
SwrContext *swrContext;

void MediaManager::pushStream() {
    
    //    FILE *pcmFile = fopen("/Users/steven/Desktop/pcm.pcm", "w+");
    char *input_url =  "/Users/steven/Movies/Video/S8.flv";
    char* aac_url = "/Users/steven/Desktop/2.aac";
    char* rtmp_url = "rtmp://localhost:1935/hls/test";
    
    avformat_network_init();
    AVFormatContext* inFmt = NULL;
    AVFormatContext* rtmpOutFmt = NULL;
    AVFormatContext* aacOutFmt = NULL;
    
    AVCodecContext* audioDecodeCtx; //音频解码器
    AVCodecContext* videoDecodeCtx; //视频解码器
    AVCodecContext* h264EnCodecContext; //h264编码器
    AVCodecContext* aacEnCodecContext; //aac编码器
    
    
    //打开输入文件，解码音频
    int ret = avformat_open_input(&inFmt, input_url, NULL, NULL);
    if (ret != 0) {
        cout<<"Open input file error"<<endl;
        return;
    }
    avformat_find_stream_info(inFmt, NULL);
    
    //查找音频信息，并打开音频解码器
    int audioIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    audioDecodeCtx = openDecoder(inFmt, audioIndex);
    //查找视频信息，并打开视频解码器
    int videoIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    videoDecodeCtx = openDecoder(inFmt, videoIndex);
    
    //打开AAC编码器
    aacEnCodecContext = openAACEncoder();
    if (aacEnCodecContext == NULL) {
        return;
    }
    //打开h264编码器
    if(openH264Encoder(&h264EnCodecContext, videoDecodeCtx->width, videoDecodeCtx->height) < 0)
        return;
    
    //打开aac写入文件上下文
    aacOutFmt = avformat_alloc_context();
    const AVOutputFormat *avOutputFormat = av_guess_format(nullptr, aac_url, nullptr);
    aacOutFmt->oformat = (AVOutputFormat*) avOutputFormat;
    AVStream *aac_stream = avformat_new_stream(aacOutFmt, aacEnCodecContext->codec);
    avcodec_parameters_from_context(aac_stream->codecpar, aacEnCodecContext);
    ret = avio_open(&aacOutFmt->pb, aac_url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        cout<<"avio open error ret = "<<ret<<endl;
        return;
    }
    ret = avformat_write_header(aacOutFmt, nullptr);
    if (ret < 0) {
        std::cout << "文件头写入失败" << std::endl;
        return;
    }
    
    
//    //初始化rtmp输出上下文
//    ret = avformat_alloc_output_context2(&rtmpOutFmt, NULL, "flv", rtmp_url);
//    if (ret < 0) {
//        cout<<"打开rtmp推流失败"<<endl;
//        return;
//    }
//    //rtmp视频通道
//    AVStream* rtmpVideoStream = avformat_new_stream(rtmpOutFmt, h264EnCodecContext->codec);
//    AVCodecParameters* rtmpVideoPar = avcodec_parameters_alloc();
//    avcodec_parameters_from_context(rtmpVideoPar, h264EnCodecContext);
//    avcodec_parameters_copy(rtmpVideoStream->codecpar, rtmpVideoPar);
//    rtmpVideoStream->codecpar->codec_tag = 0;
//    //rtmp音频通道
//    AVStream* rtmpAudioStream = avformat_new_stream(rtmpOutFmt, aacEnCodecContext->codec);
//    AVCodecParameters* rtmpAudioPar = avcodec_parameters_alloc();
//    avcodec_parameters_from_context(rtmpAudioPar, aacEnCodecContext);
//    avcodec_parameters_copy(rtmpAudioStream->codecpar, rtmpAudioPar);
//    rtmpAudioStream->codecpar->codec_tag = 0;
//    ret = avio_open(&(rtmpOutFmt->pb), rtmp_url, AVIO_FLAG_WRITE);
//    if (ret < 0) {
//        cout<<"打开rtmp server失败"<<endl;
//        return;
//    }
//    ret = avformat_write_header(rtmpOutFmt, 0);
//    if (ret < 0) {
//        cout<<"写入rtmp server header失败"<<endl;
//        return;
//    }
    
    
    //初始化缓冲区AVAudioFifo，由于mp3是1153个采样点，aac是1024个采样点，所以需要用到avaudiofifo缓冲器。
    AVAudioFifo* fifo = av_audio_fifo_alloc(aacEnCodecContext->sample_fmt, aacEnCodecContext->channels, aacEnCodecContext->frame_size);
    AVPacket* inPacket = av_packet_alloc();
    AVFrame* inFrame = av_frame_alloc();
    
    //初始化音频重采样转换器
    swrContext = getSwrContext(aacEnCodecContext->channel_layout, aacEnCodecContext->sample_fmt, aacEnCodecContext->sample_rate,
                               audioDecodeCtx->channel_layout, audioDecodeCtx->sample_fmt, audioDecodeCtx->sample_rate);
    swr_init(swrContext);
    //    uint8_t *buffer = (uint8_t *)av_malloc(192000 * 2);
    
    AVFrame* convertFrame = av_frame_alloc();
    convertFrame->channels = aacEnCodecContext->channels;
    convertFrame->channel_layout = aacEnCodecContext->channel_layout;
    convertFrame->format = aacEnCodecContext->sample_fmt;
    convertFrame->nb_samples = aacEnCodecContext->frame_size;
    convertFrame->sample_rate = aacEnCodecContext->sample_rate;
    if(av_frame_get_buffer(convertFrame, 0) < 0) {
        av_log(NULL, AV_LOG_ERROR, "alloc avframe failed");
        return;
    }
    
    //开始解码读取数据
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
        ret = avcodec_send_packet(audioDecodeCtx, inPacket);
        while (true) {
            ret = avcodec_receive_frame(audioDecodeCtx, inFrame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }else if (ret < 0) {
                cout<<"Decode audio receive frame error: ret = "<<ret<<endl;
                break;
            }
            
            //音频处理
            if (inPacket->stream_index == audioIndex) {
                
                //            //保存为pcm数据
                //            swr_convert(swrContext, &buffer, 192000, (const uint8_t**) inFrame->data, inFrame->nb_samples);
                //            int buffer_size = av_samples_get_buffer_size(NULL, 2, inFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
                //            fwrite(buffer, buffer_size, 1, pcmFile);
                
                //先转换指定格式
                swr_convert_frame(swrContext, convertFrame, inFrame);
                //开始将frame编码为aac
                encodeToAAC(aacEnCodecContext, aacOutFmt, fifo, convertFrame);
            }
        }
    }
    ret = av_write_trailer(aacOutFmt);
    cout<<"转换结束"<<endl;
    
}


void MediaManager::encodeToAAC(AVCodecContext* encodeContext, AVFormatContext* outFmt, AVAudioFifo* fifo, AVFrame* inFrame){
    
    cout<<"解码前编码格式为："<<av_get_sample_fmt_name((AVSampleFormat) inFrame->format)<<endl;
    
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
    cout<<"write fifo number = "<<ret<<endl;
    
    
    AVPacket *outPacket = av_packet_alloc();
    while(true) {
        int count = av_audio_fifo_size(fifo);
        if (count < encodeContext->frame_size) {
            break;
        }
        count = av_audio_fifo_read(fifo, reinterpret_cast<void **>(outFrame->data), encodeContext->frame_size);
        if (count < 0) {
            std::cout << "audiofifo 读取数据失败" << std::endl;
            return;
        }
        cur_pts += outFrame->nb_samples;
        outFrame->pts = cur_pts;
        
        ret = avcodec_send_frame(encodeContext, outFrame);
        while (true) {
            ret = avcodec_receive_packet(encodeContext, outPacket);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cout << "avcodec_receive_packet end:" << ret << std::endl;
                break;
            } else if (ret < 0) {
                std::cout << "avcodec_receive_packet fail:" << ret << std::endl;
                return;
            } else {
                cout<<"write file data size = "<<outPacket->size<<endl;
                //                outPacket->stream_index = aac_index;
                ret = av_write_frame(outFmt, outPacket);
                if (ret < 0) {
                    cout << "av_write_frame fail:" << ret << endl;
                    return;
                } else {
                    cout << "av_write_frame success:" << ret << endl;
                }
            }
        }
    }
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


int MediaManager::openH264Encoder(AVCodecContext **codecContext, int width, int height) {
    AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if(codec == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Not found codec \n");
        return -1;
    }
    
    *codecContext = avcodec_alloc_context3(codec);
    if(codecContext == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Not found AVCodecContext \n");
        return -1;
    }
    
    (*codecContext)->pix_fmt = AV_PIX_FMT_YUV420P;
    //分辨率
    (*codecContext)->width = width;
    (*codecContext)->height = height;
    //帧率、码率
    (*codecContext)->framerate = AVRational{25, 1};
    (*codecContext)->time_base = AVRational{1, 25};
    (*codecContext)->bit_rate = 600000;
    //GOP
    (*codecContext)->gop_size = 10;
    (*codecContext)->keyint_min = 5;
    //B帧
    (*codecContext)->max_b_frames = 3;
    (*codecContext)->has_b_frames = 1;
    //参考帧
    (*codecContext)->refs = 3;
    //SPS/PPS
    (*codecContext)->profile = FF_PROFILE_H264_HIGH_444;
    (*codecContext)->level = 50;
    
    //设置编码速度，越慢质量越好
    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set((*codecContext)->priv_data, "preset", "slow", 0);
    
    if(avcodec_open2(*codecContext, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Open x264 encoder error \n");
        return -1;
    }
    return 0;
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
