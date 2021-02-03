//
// Created by Zach on 2020/12/29.
//



#include "VideoChannel.h"


extern "C" {
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

VideoChannel::VideoChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                           const AVRational &base, int fps) :
    BaseChannel(channelId, helper, avCodecContext, base), fps(fps) {

    pthread_mutex_init(&surfaceMutex, 0);

}


VideoChannel::~VideoChannel() {
    pthread_mutex_destroy(&surfaceMutex);
}


void *videoDecode_t(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->decode();
    return 0;
}

void *videoPlay_t(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->_play();

    return 0;
}

void VideoChannel::_play() {
    SwsContext *swsContext = sws_getContext(avCodecContext->width,
                                    avCodecContext->height,
                                    avCodecContext->pix_fmt,
                                    avCodecContext->width,
                                    avCodecContext->height,
                                    AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR,
                                    0, 0, 0);

    uint8_t *data[4];
    int linesize[4];
    av_image_alloc(data, linesize, avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA, 1);

    AVFrame *frame = 0;
    int ret;
    while (isPlaying) {
        ret = frame_queue.deQueue(frame);
        if (!isPlaying) {  // 前面deQueue阻塞之后可能已经停止播放
            break;
        }

        if (!ret) {
            continue;
        }

        // 2 指针数据，比如RGBA。 每一个维度是一个指针，RGBA需要4个指针，所以是4个指针组成的数组
        // 3 每一行数据的数据个数
        // 4 offset
        // 5 要转化的图像的高
        sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, data, linesize);


        onDraw(data, linesize, avCodecContext->width, avCodecContext->height);
        releaseAvFrame(frame);  // important
    }

    av_free(&data[0]);
    isPlaying = 0;
    releaseAvFrame(frame);
    sws_freeContext(swsContext);

}

void VideoChannel::onDraw(uint8_t **data, int *linesize, int width, int height) {
    pthread_mutex_lock(&surfaceMutex);
    if (!window) {
        pthread_mutex_unlock(&surfaceMutex);
        return;
    }

    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer buffer;

    if (ANativeWindow_lock(window, &buffer, 0) != 0) {
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&surfaceMutex);
        return;
    }

    // 把视频数据刷到buffer中
    uint8_t *dstData = static_cast<uint8_t *>(buffer.bits);
    int dstSize = buffer.stride * 4;

    // 视频图像的源RGBA数据
    uint8_t *srcData = data[0];
    int srcSize = linesize[0];

    for (int i = 0; i < buffer.height; i++) {
        memcpy(dstData + i * dstSize, srcData + i * srcSize, srcSize);
    }

    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&surfaceMutex);
}

void VideoChannel::play() {
    isPlaying = 1;
    setEnable(true);

    pthread_create(&videoDecodeTask, 0, videoDecode_t, this);
    pthread_create(&videoPlayTask, 0, videoPlay_t, this);
}

void VideoChannel::stop() {

}

void VideoChannel::decode() {
    AVPacket *packet = 0;
    while (isPlaying) {

        // 阻塞
        int ret = pkt_queue.deQueue(packet);
        if (!isPlaying) {
            break;
        }


        if (!ret) {
            continue;
        }

        // 向解码器发送解码数据
        ret = avcodec_send_packet(avCodecContext, packet);
        releaseAvPacket(packet);
        if (ret < 0) {
            break;
        }

        // 从解码器取出解码好的数据
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            break;
        }
        frame_queue.enQueue(frame);
    }
    releaseAvPacket(packet);
}

void VideoChannel::setWindow(ANativeWindow *window) {
    pthread_mutex_lock(&surfaceMutex);
    if (this->window) {
        ANativeWindow_release(this->window);
    }
    this->window = window;

    pthread_mutex_unlock(&surfaceMutex);
}
