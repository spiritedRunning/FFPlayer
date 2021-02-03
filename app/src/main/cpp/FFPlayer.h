//
// Created by Zach on 2020/12/21.
//

#ifndef FFPLAYER_FFPLAYER_H
#define FFPLAYER_FFPLAYER_H


#include <pthread.h>
#include <android/native_window_jni.h>

#include "JavaCallHelper.h"
#include "VideoChannel.h"
#include "AudioChannel.h"

extern "C" {
#include <libavformat/avformat.h>
};

class FFPlayer {
    friend void *prepare_t(void *args);
    friend void *start_t(void *args);


public:
    FFPlayer(JavaCallHelper *helper);

    void setDataSource(const char* path);
    void prepare();
    void start();

    void setWindow(ANativeWindow *window);


private:
    void _prepare();
    void _start();

private:
    char* path;
    pthread_t prepareTask;
    JavaCallHelper *helper;
    int64_t duration;

    VideoChannel *videoChannel;
    AudioChannel *audioChannel;

    pthread_t startTask;
    bool isPlaying;
    AVFormatContext *avFormatContext;

    ANativeWindow *window = 0;

};


#endif //FFPLAYER_FFPLAYER_H
