//
// Created by Zach on 2020/12/24.
//

#ifndef FFPLAYER_BASECHANNEL_H
#define FFPLAYER_BASECHANNEL_H


#include "JavaCallHelper.h"
#include "safe_queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class BaseChannel {
public:
    BaseChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext, AVRational base) :
        channelId(channelId),
        helper(helper),
        avCodecContext(avCodecContext),
        time_base(base) {

        pkt_queue.setReleaseHandle(releaseAvPacket);
        frame_queue.setReleaseHandle(releaseAvFrame);
    }

    virtual ~BaseChannel() {
        if (avCodecContext) {
            avcodec_close(avCodecContext);
            avcodec_free_context(&avCodecContext);
            avCodecContext = 0;
        }
        pkt_queue.clear();
        frame_queue.clear();
    }

    virtual void play() = 0;

    virtual void stop() = 0;

    virtual void decode() = 0;

    void setEnable(bool enable) {
        pkt_queue.setEnable(enable);
        frame_queue.setEnable(enable);
    }


    static void releaseAvFrame(AVFrame *&frame) {
        if (frame) {
            av_frame_free(&frame);
            frame = 0;
        }
    }

    static void releaseAvPacket(AVPacket *&packet) {
        if (packet) {
            av_packet_free(&packet);
            packet = 0;
        }
    }

public:
    int channelId;
    JavaCallHelper *helper;
    AVCodecContext *avCodecContext;
    AVRational time_base;

    bool isPlaying = false;
    double clock = 0;

    SafeQueue<AVPacket *> pkt_queue;
    SafeQueue<AVFrame *> frame_queue;
};


#endif //FFPLAYER_BASECHANNEL_H
