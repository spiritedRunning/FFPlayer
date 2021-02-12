//
// Created by Administrator on 2021/1/26.
//

#ifndef FFPLAYER_AUDIOCHANNEL_H
#define FFPLAYER_AUDIOCHANNEL_H


extern "C" {
#include <libswresample/swresample.h>

};

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class AudioChannel : public BaseChannel {
    friend void *audioPlay_t(void *args);
    friend void bqPlayerCallback(SLAndroidSimpleBufferQueueItf queue, void *pContext);

public:
    AudioChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                 const AVRational &base);

    virtual ~AudioChannel();

    virtual void play();

    virtual void stop();

    virtual void decode();

private:
    void play_t();

    int getData_t();
    void _releaseOpenSL();


private:
    pthread_t audioDecodeTask, audioPlayTask;
    SwrContext *swrContext = 0;
    uint8_t *buffer;
    int bufferCount;
    int out_channels;
    int out_samplesSize;

    SLObjectItf engineObject = NULL;
    SLEngineItf engineInterface = NULL;
    SLObjectItf outputMixObject = NULL;

    SLObjectItf bqPlayerObject = NULL;
    SLPlayItf bqPlayerInterface = NULL;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;


};


#endif //FFPLAYER_AUDIOCHANNEL_H
