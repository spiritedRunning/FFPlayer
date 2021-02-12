//
// Created by Zach on 2020/12/29.
//



#include "VideoChannel.h"
#include "Log.h"


extern "C" {
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.1

VideoChannel::VideoChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                           const AVRational &base, double fps) :
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
    av_image_alloc(data, linesize, avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA,
                   1);

    AVFrame *frame = 0;
    int ret;
    double frame_delay = 1.0 / fps;
    while (isPlaying) {
        ret = frame_queue.deQueue(frame);
        if (!isPlaying) {  // 前面deQueue阻塞之后可能已经停止播放
            break;
        }

        if (!ret) {
            continue;
        }

        // 让视频流畅播放
        double extra_delay = frame->repeat_pict / (2 * fps);
        double delay = extra_delay + frame_delay;

        if (audioChannel) {
            clock = frame->best_effort_timestamp * av_q2d(time_base);
            double diff = clock - audioChannel->clock;

            /**
             * 1. delay < 0.04,  同步阈为0.04
             * 2. delay > 0.1, 同步阈值为0.1
             * 3. 0.04 < delay < 0.1 同步阈值为delay
             */
            // 根据每秒视频的播放数，确定音视频播放的允许范围
            double sync = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
            // 视频落后了
            if (diff <= -sync) {
                // 让delay时间减小
                delay = FFMAX(0, delay + diff);
            } else if (diff > sync) {
                // 视频快了
                delay = delay + diff;
            }

            LOGE("Video:%lf Audio:%lf delay:%lf A-V=%lf", clock, audioChannel->clock, delay, -diff);
        }

        av_usleep(delay * 1000000);

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
    isPlaying = 0;
    helper = 0;
    setEnable(0);
    pthread_join(videoDecodeTask, 0);
    pthread_join(videoPlayTask, 0);

    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
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

        // important 更好的实现时需要时 再解码，只需要一个线程与一个待解码队列。 不太懂？？
        while (frame_queue.size() > fps * 10 && isPlaying) {
            av_usleep(1000 * 10);
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
