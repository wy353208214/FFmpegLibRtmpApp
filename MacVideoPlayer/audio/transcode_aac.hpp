//
//  transcode_aac.hpp
//  MacVideoPlayer
//
//  Created by steven on 2022/11/1.
//

#ifndef transcode_aac_hpp
#define transcode_aac_hpp

#include <stdio.h>


#include <iostream>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/channel_layout.h"
}

class Mp3ToAAC {
public:
    void mp3_to_aac(const char *mp3, const char *aac) {
        avFormatContext = avformat_alloc_context();
        int ret = avformat_open_input(&avFormatContext, mp3, nullptr, nullptr);
        if (ret < 0) {
            std::cout << "打开mp3输入流失败" << std::endl;
            return;
        }
        av_dump_format(avFormatContext, 0, mp3, 0);
        avformat_find_stream_info(avFormatContext, NULL);
        std::cout << "流的类型:" << avFormatContext->streams[0]->codecpar->codec_type << std::endl;
        
        int audio_index = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audio_index < 0) {
            std::cout << "没有找到音频流，换一个方式查找" << std::endl;
            for (int i = 0; i < avFormatContext->nb_streams; ++i) {
                if (AVMEDIA_TYPE_AUDIO == avFormatContext->streams[i]->codecpar->codec_type) {
                    audio_index = i;
                    std::cout << "找到音频流，audio_index:" << audio_index << std::endl;
                    break;
                }
            }
            
            if (audio_index < 0) {
                return;
            }
        }
        
        // 初始化输出
        out_format_context = avformat_alloc_context();
        const AVOutputFormat *avOutputFormat = av_guess_format(nullptr, aac, nullptr);
        out_format_context->oformat = (AVOutputFormat*) avOutputFormat;
        const AVCodec *aac_encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
        encode_CodecContext = avcodec_alloc_context3(aac_encoder);
        
        encode_CodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
        encode_CodecContext->channels = av_get_channel_layout_nb_channels(encode_CodecContext->channel_layout);
        // 如果解码出来的pcm不是44100的则需要进行重采样，重采样需要主要音频时长不变
        encode_CodecContext->sample_rate = 44100;
        // 比如使用 22050的采样率进行编码，编码后的时长明显是比实际音频长的
        //        encode_CodecContext->sample_rate = 22050;
        encode_CodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
        encode_CodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        encode_CodecContext->profile = FF_PROFILE_AAC_LOW;
        //ffmpeg默认的aac是不带adts，而fdk_aac默认带adts，这里我们强制不带
        encode_CodecContext->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        // 打开编码器
        ret = avcodec_open2(encode_CodecContext, aac_encoder, nullptr);
        if (ret < 0) {
            char error[1024];
            av_strerror(ret, error, 1024);
            std::cout << "编码器打开失败：" << error << std::endl;
            return;
        }
        
        AVStream *aac_stream = avformat_new_stream(out_format_context, aac_encoder);
        aac_index = aac_stream->index;
        avcodec_parameters_from_context(aac_stream->codecpar, encode_CodecContext);
        ret = avio_open(&out_format_context->pb, aac, AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cout << "输出流打开失败" << std::endl;
            return;
        }
        ret = avformat_write_header(out_format_context, nullptr);
        if (ret < 0) {
            std::cout << "文件头写入失败" << std::endl;
            return;
        }
        
        // 解码相关
        const AVCodec *decoder = avcodec_find_decoder(avFormatContext->streams[audio_index]->codecpar->codec_id);
        avCodecContext = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(avCodecContext, avFormatContext->streams[audio_index]->codecpar);
        ret = avcodec_open2(avCodecContext, decoder, nullptr);
        if (ret < 0) {
            std::cout << "解码器打开失败" << std::endl;
            return;
        }
        // 分配frame和pack
        AVPacket *avPacket = av_packet_alloc();
        avFrame = av_frame_alloc();
        out_pack = av_packet_alloc();
        
        encode_frame = av_frame_alloc();
        encode_frame->nb_samples = encode_CodecContext->frame_size;
        encode_frame->sample_rate = encode_CodecContext->sample_rate;
        encode_frame->channel_layout = encode_CodecContext->channel_layout;
        encode_frame->channels = encode_CodecContext->channels;
        encode_frame->format = encode_CodecContext->sample_fmt;
        av_frame_get_buffer(encode_frame, 0);
        
        // 初始化audiofifo
        audiofifo = av_audio_fifo_alloc(encode_CodecContext->sample_fmt, encode_CodecContext->channels,
                                        encode_CodecContext->frame_size);
        while (true) {
            ret = av_read_frame(avFormatContext, avPacket);
            if (ret < 0) {
                std::cout << "read end" << std::endl;
                break;
            } else if (avPacket->stream_index == audio_index) {
                decode_to_pcm(avPacket);
            }
            av_packet_unref(avPacket);
        }
        
        // todo 需要冲刷数据，不然可能会漏掉几帧数据 [aac @ 0x125e05f50] 2 frames left in the queue on closing
        
        ret = av_write_trailer(out_format_context);
        if (ret < 0) {
            std::cout << "文件尾写入失败" << std::endl;
        }
    }
    
    ~Mp3ToAAC() {
        if (nullptr != avFormatContext) {
            avformat_close_input(&avFormatContext);
        }
        avcodec_free_context(&avCodecContext);
        
        if (nullptr != out_format_context) {
            avformat_close_input(&out_format_context);
        }
        avcodec_free_context(&encode_CodecContext);
    }
    
private:
    // 解码
    AVFormatContext *avFormatContext = nullptr;
    AVCodecContext *avCodecContext = nullptr;
    AVFrame *avFrame = nullptr;
    
    // 编码
    AVFormatContext *out_format_context = nullptr;
    AVCodecContext *encode_CodecContext = nullptr;
    AVPacket *out_pack = nullptr;
    AVFrame *encode_frame = nullptr;
    AVAudioFifo *audiofifo = nullptr;
    
    int aac_index = 0;
    int64_t cur_pts = 0;
    
    void encode_to_aac() {
        av_frame_make_writable(encode_frame);
        // todo 如果是冲刷最后几帧数据，不够的可以填充静音  av_samples_set_silence
        while (av_audio_fifo_size(audiofifo) > encode_CodecContext->frame_size) {
            int ret = av_audio_fifo_read(audiofifo, reinterpret_cast<void **>(encode_frame->data),
                                         encode_CodecContext->frame_size);
            if (ret < 0) {
                std::cout << "audiofifo 读取数据失败" << std::endl;
                return;
            }
            cur_pts += encode_frame->nb_samples;
            encode_frame->pts = cur_pts;
            ret = avcodec_send_frame(encode_CodecContext, encode_frame);
            if (ret < 0) {
                std::cout << "发送编码失败" << std::endl;
                return;
            }
            while (true) {
                ret = avcodec_receive_packet(encode_CodecContext, out_pack);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    std::cout << "avcodec_receive_packet end:" << ret << std::endl;
                    break;
                } else if (ret < 0) {
                    std::cout << "avcodec_receive_packet fail:" << ret << std::endl;
                    return;
                } else {
                    out_pack->stream_index = aac_index;
                    ret = av_write_frame(out_format_context, out_pack);
                    if (ret < 0) {
                        std::cout << "av_write_frame fail:" << ret << std::endl;
                        return;
                    } else {
                        std::cout << "av_write_frame success:" << ret << std::endl;
                    }
                }
            }
        }
    }
    
    void decode_to_pcm(const AVPacket *avPacket) {
        int ret = avcodec_send_packet(avCodecContext, avPacket);
        if (ret < 0) {
            char error[1024];
            av_strerror(ret, error, 1024);
            std::cout << "发送解码失败:" << error << std::endl;
        }
        while (true) {
            ret = avcodec_receive_frame(avCodecContext, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cout << "avcodec_receive_frame end:" << ret << std::endl;
                break;
            } else if (ret < 0) {
                std::cout << "获取解码数据失败" << std::endl;
                return;
            } else {
                std::cout << "获取解码数据成功 nb_samples:" << avFrame->nb_samples << std::endl;
                /**
                 * mp3解码出来的pcm无法直接编码成aac，
                 * 因为mp3每帧是1152个采样点，而aac每帧需要1024个采样点
                 * 解决方案是使用AVAudioFifo缓冲起来
                 */
                
                int cache_size = av_audio_fifo_size(audiofifo);
                std::cout << "cache_size:" << cache_size << std::endl;
                int re = av_audio_fifo_realloc(audiofifo, cache_size + avFrame->nb_samples);
                av_audio_fifo_write(audiofifo, reinterpret_cast<void **>(avFrame->data), avFrame->nb_samples);
                encode_to_aac();
            }
        }
    }
};



#endif /* transcode_aac_hpp */
