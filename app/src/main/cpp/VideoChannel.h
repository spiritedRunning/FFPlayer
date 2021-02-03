//
// Created by Zach on 2020/12/29.
//

#ifndef FFPLAYER_VIDEOCHANNEL_H
#define FFPLAYER_VIDEOCHANNEL_H


#include <android/native_window.h>
#include "BaseChannel.h"
extern "C" {
#include <libavcodec/avcodec.h>
};

class VideoChannel : public BaseChannel {
    friend void *videoPlay_t(void *args);


public:
    VideoChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext, const AVRational &base, int fps);

    virtual ~VideoChannel();

    void setWindow(ANativeWindow *window);

public:
    virtual void play();

    virtual void stop();

    virtual void decode();

private:
    void _play();

private:
    int fps;
    pthread_t videoDecodeTask, videoPlayTask;
    bool isPlaying;
    pthread_mutex_t surfaceMutex;

    ANativeWindow *window = 0;

    void onDraw(uint8_t *data[4], int linesize[4], int width, int height);
};


#endif //FFPLAYER_VIDEOCHANNEL_H
