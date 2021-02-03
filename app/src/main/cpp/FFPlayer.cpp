//
// Created by zach on 2020/12/21.
//

#include "FFPlayer.h"
#include "Log.h"
#include <cstring>


extern "C" {
#include "libavformat/avformat.h"
}

FFPlayer::FFPlayer(JavaCallHelper *helper) : helper(helper) {
    avformat_network_init();
    videoChannel = 0;
    audioChannel = 0;
}



void FFPlayer::setDataSource(const char *path_) {
    path = new char[strlen(path_) + 1];
    strcpy(path, path_);
}


void *prepare_t(void *args) {
    FFPlayer *player = static_cast<FFPlayer *>(args);
    player->_prepare();
    return 0;

}


void FFPlayer::prepare() {
    pthread_create(&prepareTask, 0, prepare_t, this);
}

void FFPlayer::_prepare() {
    avFormatContext = avformat_alloc_context();

    /**
     * 1、打开媒体文件
     */
    // 参数3： 输入文件的封装格式， null表示自动检测格式
    // 参数4： map集合，进行一些配置
    int ret = avformat_open_input(&avFormatContext, path, 0, 0);
    if (ret != 0) {
        LOGE("open %s failed, return %d, error: %s", path, ret, av_err2str(ret));
        helper->onError(FFMPEG_CAN_NOT_OPEN_URL, THREAD_CHILD);
        return;
    }

    /**
     * 2、查找媒体流
     */
    ret = avformat_find_stream_info(avFormatContext, 0);
    if (ret < 0) {
        LOGE("find stream %s failed, return %d, error: %s", path, ret, av_err2str(ret));
        helper->onError(FFMPEG_CAN_NOT_FIND_STREAMS, THREAD_CHILD);
        return;
    }
    duration = avFormatContext->duration / AV_TIME_BASE;

    for (int i = 0; i < avFormatContext->nb_streams; i++) {
        AVStream *avStream = avFormatContext->streams[i];
        // 解码信息
        AVCodecParameters *parameters = avStream->codecpar;
        AVCodec *dec = avcodec_find_decoder(parameters->codec_id);
        if (!dec) {
            helper->onError(FFMPEG_FIND_DECODER_FAIL, THREAD_CHILD);
            return;
        }

        AVCodecContext *codecContext = avcodec_alloc_context3(dec);
        // 解码信息赋值给解码上下文的各种成员
        if (avcodec_parameters_to_context(codecContext, parameters) < 0) {
            helper->onError(FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL, THREAD_CHILD);
            return;
        }

        if (avcodec_open2(codecContext, dec, 0) != 0) {
            helper->onError(FFMPEG_OPEN_DECODER_FAIL, THREAD_CHILD);
            return;
        }

        if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioChannel = new AudioChannel(i, helper, codecContext, avStream->time_base);

        } else if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            // 帧率
            double fps = av_q2d(avStream->avg_frame_rate);
            // important  防止获取不到fps
            if (isnan(fps) || fps == 0) {
                fps = av_q2d(avStream->r_frame_rate);
            }
            if (isnan(fps) || fps == 0) {
                fps = av_q2d(av_guess_frame_rate(avFormatContext, avStream, 0));
            }

            videoChannel = new VideoChannel(i, helper, codecContext, avStream->time_base, fps);
            videoChannel->setWindow(window);
        }
    }


    if (!videoChannel && !audioChannel) {
        helper->onError(FFMPEG_NOMEDIA, THREAD_CHILD);
        return;
    }

    helper->onPrepare(THREAD_CHILD);

}


void *start_t(void *args) {
    FFPlayer *player = static_cast<FFPlayer *>(args);
    player->_start();
    return 0;

}

void FFPlayer::start() {
    isPlaying = true;
    if (videoChannel) {
        videoChannel->play();
    }

    if (audioChannel) {
        audioChannel->play();
    }
    pthread_create(&startTask, 0, start_t, this);

}

void FFPlayer::_start() {
    int ret;
    while (isPlaying) {
        AVPacket *packet = av_packet_alloc();
        // important: 这里avFormatContext 一定要是全局的！！
        ret = av_read_frame(avFormatContext, packet);
        if (ret == 0) {
            if (videoChannel && packet->stream_index == videoChannel->channelId) {
                videoChannel->pkt_queue.enQueue(packet);
            } else if (audioChannel && packet->stream_index == audioChannel->channelId) {
                audioChannel->pkt_queue.enQueue(packet);
            } else { // 多路的情况不处理
                av_packet_free(&packet);
            }
        } else if (ret == AVERROR_EOF) {
            // 读取完毕，不一定播放完毕
            if (videoChannel->pkt_queue.empty() && videoChannel->frame_queue.empty()) {
                // 播放完毕
                break;
            }

        } else {
            break;
        }
    }

    isPlaying = 0;
    videoChannel->stop();
    audioChannel->stop();
}

void FFPlayer::setWindow(ANativeWindow *window) {
    this->window = window;
    if (videoChannel) {
        videoChannel->setWindow(window);
    }
}





