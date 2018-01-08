//
//  PlayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "PlayController.hpp"
#include "DebugFuncs.h"
#include "ThreadConvience.h"

using namespace tfmpcore;

bool PlayController::connectAndOpenMedia(std::string mediaPath){
    
    this->mediaPath = mediaPath;
    
    av_register_all();
    
    fmtCtx = avformat_alloc_context();
    int retval = avformat_open_input(&fmtCtx, mediaPath.c_str(), NULL, NULL);
    TFCheckRetvalAndGotoFail("avformat_open_input");
    
    //configure options to get faster
    retval = avformat_find_stream_info(fmtCtx, NULL);
    TFCheckRetvalAndGotoFail("avformat_find_stream_info");
    
    for (int i = 0; i<fmtCtx->nb_streams; i++) {
        
        AVMediaType type = fmtCtx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
            videoDecoder = new Decoder(fmtCtx, i, type);
        }else if (type == AVMEDIA_TYPE_AUDIO){
            audioDecoder = new Decoder(fmtCtx, i, type);
        }else if (type == AVMEDIA_TYPE_SUBTITLE){
            subtitleDecoder = new Decoder(fmtCtx, i, type);
        }
    }
    
    if (videoDecoder && !videoDecoder->prepareDecode()) {
        return false;
    }
    
    if (audioDecoder && !audioDecoder->prepareDecode()) {
        return false;
    }
    
    if (subtitleDecoder && !subtitleDecoder->prepareDecode()) {
        return false;
    }
    
    //check whether stream can display.
    if ((displayMediaType & TFMP_MEDIA_TYPE_VIDEO) && videoDecoder != nullptr && displayVideoFrame == nullptr) {
        return false;
    }else{
        displayer->displayVideoFrame = displayVideoFrame;
    }
    
    displayer->displayContext = displayContext;
    displayer->shareVideoBuffer = videoDecoder->sharedFrameBuffer();
    displayer->shareAudioBuffer = audioDecoder->sharedFrameBuffer();
    
    displayer->syncClock = new SyncClock(isAudioMajor);

    prapareOK = true;
    
    if (connectCompleted != nullptr) {
        connectCompleted(this);
    }
    
    return true;
    
fail:
    return false;
}

void PlayController::play(){
    if (!prapareOK) {
        return;
    }
    
    startReadingFrames();
    if (videoDecoder) {
        videoDecoder->startDecode();
    }
    if (audioDecoder) {
        audioDecoder->startDecode();
    }
    if (subtitleDecoder) {
        subtitleDecoder->startDecode();
    }
    
    displayer->startDisplay();
}

void PlayController::pause(){
    
}

void PlayController::stop(){
    if (videoDecoder) {
        videoDecoder->stopDecode();
    }
    if (audioDecoder) {
        audioDecoder->stopDecode();
    }
    if (subtitleDecoder) {
        subtitleDecoder->stopDecode();
    }
    
    displayer->stopDisplay();
}

/***** properties *****/

void PlayController::setDisplayMediaType(TFMPMediaType displayMediaType){
    this->displayMediaType = displayMediaType;
    displayer->displayMediaType = displayMediaType;
}

FillAudioBufferStruct PlayController::getFillAudioBufferStruct(){
    return displayer->getFillAudioBufferStruct();
}

/***** private ******/

void PlayController::startReadingFrames(){
    pthread_create(&readThread, nullptr, readFrame, this);
}

void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket *packet = av_packet_alloc();
    
    while (av_read_frame(controller->fmtCtx, packet) == 0) {
        
        if (packet->stream_index == controller->videoStrem) {
            
            controller->videoDecoder->decodePacket(packet);
            
        }else if (packet->stream_index == controller->audioStream){
            
            controller->audioDecoder->decodePacket(packet);
            
        }else if (packet->stream_index == controller->subTitleStream){
            
            controller->subtitleDecoder->decodePacket(packet);
        }
    }
    
    return 0;
}

