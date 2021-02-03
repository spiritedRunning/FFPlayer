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


private:
    pthread_t audioDecodeTask, audioPlayTask;
    SwrContext *swrContext = 0;
    uint8_t *buffer;
    int bufferCount;
    int out_channels;
    int out_samplesSize;

};


#endif //FFPLAYER_AUDIOCHANNEL_H
