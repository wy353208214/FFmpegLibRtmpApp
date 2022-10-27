//
//  publisher.cpp
//  MacVideoPlayer
//
//  Created by steven on 2022/10/24.
//

#include "publisher.hpp"
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

#define threeByteToLittle(x) (x >> 16) | ((x & 0x0000ff) << 16) | (x & 0x00ff00);

RTMP* Publisher::connect(char *url){
    rtmp = RTMP_Alloc();
    if (rtmp == NULL) {
        printf("Alloc rtmp失败 \n");
        return NULL;
    }
    RTMP_Init(rtmp);
    rtmp->Link.timeout = 10;
    if (!RTMP_SetupURL(rtmp, url)) {
        printf("Setup URL 失败 \n");
        return NULL;
    }
    RTMP_EnableWrite(rtmp);
    if(!RTMP_Connect(rtmp, NULL)){
        printf("Connect 失败 \n");
        return NULL;
    }
    if(!RTMP_ConnectStream(rtmp, 0)){
        printf("Connect Stream 失败 \n");
        return NULL;
    }
    
    return rtmp;
}

void Publisher::sendPacket(AVPacket *avPacket){
    if (!RTMP_IsConnected(rtmp)) {
        cout<<"Rtmp is not connected"<<endl;
        return;
    }
    //全局packet，第一次判断是否为null
    if (packet == NULL) {
        packet = new RTMPPacket();
        RTMPPacket_Alloc(packet, 64 * 1024);
        RTMPPacket_Reset(packet);
        packet->m_hasAbsTimestamp = 0;
        packet->m_nChannel = 0x4;
        packet->m_nInfoField2 = rtmp->m_stream_id;
    }
    
    //读取AVPacket中的数据，转换成RTMPPacket
    memcpy(packet->m_body, avPacket->data, avPacket->size);
    packet->m_nBodySize = avPacket->size;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nTimeStamp = (uint32_t) avPacket->pts;
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    
    
    //发送rtmp数据包
    RTMP_SendPacket(rtmp, packet, 0);
    
}

void Publisher::disConnect(){
    if (rtmp) {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }
}


void Publisher::readData(char* filePath) {
    
//    isBigEnd();
    
    
    //创建RTMPPacket
    if (packet == NULL) {
        packet = new RTMPPacket();
    }
    RTMPPacket_Alloc(packet, 64 * 1024);
    RTMPPacket_Reset(packet);
    packet->m_hasAbsTimestamp = 0;
    packet->m_nChannel = 0x4;
    packet->m_nInfoField2 = rtmp->m_stream_id;
    
    
    FILE *file = fopen(filePath, "rb");
    //计算file长度
    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    //跳过flv header 9字节
    fseek(file, 9, SEEK_CUR);
    //跳过First Pre Tag 4字节
    fseek(file, 4, SEEK_CUR);
    
    uint32_t *type = new uint32_t;
    uint32_t *size = new uint32_t;
    uint32_t *time_stamp = new uint32_t;
    uint32_t *time_stamp_ext = new uint32_t;
    uint32_t *stream_id = new uint32_t;
    uint32_t *pre_tag = new uint32_t;
    
    int pre_time_stamp = 0;
    int total = 0;
    int index = 0;
    
    while (len != ftell
           (file)) {
        //读取Tag
        fread(type, 1, 1, file);
        fread(size, 1, 3, file);
        fread(time_stamp, 1, 3, file);
        fread(time_stamp_ext, 1, 1, file);
        fread(stream_id, 1, 3, file);
        
        *size = threeByteToLittle(*size);
        *time_stamp = threeByteToLittle(*time_stamp);
        
        //Seek 会消除feof标志位，所以需要用fread，否则无法跳出循环
//        fseek(file, *size, SEEK_CUR);
        
        fread(packet->m_body, *size, 1, file);
        packet->m_nTimeStamp = *time_stamp;
        packet->m_nBodySize = *size;
        packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet->m_packetType = *type;
        
        if(!RTMP_IsConnected(rtmp)) {
            cout<<"Rtmp is not connected"<<endl;
            break;
        }
        
        int diff = *time_stamp - pre_time_stamp;
        pre_time_stamp = *time_stamp;
        total += diff;

        
        this_thread::sleep_for(chrono::milliseconds(diff));
        if(!RTMP_SendPacket(rtmp, packet, 0)) {
            cout<<"Send packet failed"<<endl;
        }
        
        printf("%d . flv type is %d, data size is %d, time stamp is %d, stream id is %d \n", index++, *type, *size, *time_stamp, *stream_id);
        //读取pre_tag
        fread(pre_tag, 4, 1, file);
    
    }
    
    cout<<"total = "<<total<<endl;
    printf("Read end");
    delete type;
    delete size;
    delete time_stamp;
    delete time_stamp_ext;
    delete stream_id;
}

bool Publisher::isBigEnd() {
    uint64_t flag = 2;
    uint64_t *raw_ptr = &flag;
    char *p = reinterpret_cast<char*>(raw_ptr);
    
    bool isBigEnd  = *p != 2;
    if(isBigEnd) {
        cout << "big" << endl;
    } else {
        cout << "little" << endl;
    }
    return isBigEnd;
}


int32_t Publisher::swapBit(int32_t p) {
    int32_t result = ((p & 0xff000000) >> 24) |
                    ((p & 0x00ff0000) >> 8) |
                    ((p & 0x0000ff00) << 8) |
                    ((p & 0x000000ff) << 24);
    return result;
}

void Publisher::publish(){
    char *serverUrl = "rtmp://localhost:1935/live/test";
    char *flvPath = "/Users/steven/Desktop/out.flv";
    
    if(connect(serverUrl) == NULL) {
        return;
    }
    readData(flvPath);
    disConnect();
}
