//
// Created by Administrator on 2021/1/26.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                           const AVRational &base) : BaseChannel(channelId, helper, avCodecContext,
                                                                 base) {
    // 2
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    // 采样位 2字节
    out_samplesSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    // 采样率
    int out_sampleRate = 44100;

    // 计算转换后数据的最大字节数
    bufferCount = out_sampleRate * out_samplesSize * out_channels;
    buffer = static_cast<uint8_t *>(malloc((size_t) bufferCount));
}

AudioChannel::~AudioChannel() {

}


void *audioDecode_t(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->decode();

    return 0;
}

void *audioPlay_t(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->play_t();

    return 0;
}


void AudioChannel::play() {
    swrContext = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
                                    avCodecContext->channel_layout,
                                    avCodecContext->sample_fmt,
                                    avCodecContext->sample_rate,
                                    0, 0);
    swr_init(swrContext);

    isPlaying = true;
    setEnable(true);


    pthread_create(&audioDecodeTask, 0, audioDecode_t, this);
    pthread_create(&audioPlayTask, 0, audioPlay_t, this);

}

void AudioChannel::stop() {

}

void AudioChannel::decode() {
    AVPacket *packet = 0;
    while (isPlaying) {
        int ret = pkt_queue.deQueue(packet);
        if (!isPlaying) {
            break;
        }

        if (!ret) {
            continue;
        }

        ret = avcodec_send_packet(avCodecContext, packet);
        releaseAvPacket(packet);

        if (ret < 0) {
            break;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            break;
        }
        frame_queue.enQueue(frame);
    }

}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf queue, void *pContext) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(pContext);
    int dataSize = audioChannel->getData_t();
    if (dataSize > 0) {
        (*queue)->Enqueue(queue, audioChannel->buffer, dataSize);
    }
}

void AudioChannel::play_t() {
    /**
     * 1、 创建引擎
     */
    SLObjectItf engineObject = NULL;
    SLresult result;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        return;
    }

    // 初始化
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        return;
    }

    SLEngineItf engineInterface = NULL;
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (result != SL_RESULT_SUCCESS) {
        return;
    }


    /**
     * 2、创建混音器
     */
    SLObjectItf outputMixObject = NULL;
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);
    if (result != SL_RESULT_SUCCESS) {
        return;
    }

    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        return;
    }


    /**
     * 3、创建播放器
     */
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};

    // PCM数据格式： pcm, 声道数,采样率，采样位，容器大小，通道掩码（双声道），字节序
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN
    };
    // 数据源: 数据获取器 + 格式
    SLDataSource slDataSource = {&android_queue, &pcm};
    // 设置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};

    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    // 播放器相当于对混音器进行了一层包装， 提供了额外的如：开始，停止
    // 真正播放声音的是混音器
    SLObjectItf bqPlayerObject = NULL;
    (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &slDataSource,
                                          &audioSnk, 1, ids, req);
    // 初始化播放器
    (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);

    // 获取播放器数据队列操作接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    // 启动播放器后，执行回调来获取数据并播放
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    SLPlayItf bqPlayerInterface = NULL;
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerInterface);
    (*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_PLAYING);


    // 还需要手动调用一次回调，才能启动播放
    bqPlayerCallback(bqPlayerBufferQueue, this);

}

int AudioChannel::getData_t() {
    int dataSize;
    AVFrame *frame = 0;

    while (isPlaying) {
        int ret = frame_queue.deQueue(frame);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }

        int nb = swr_convert(swrContext, &buffer, bufferCount,
                             (const uint8_t **) frame->data, frame->nb_samples);
        dataSize = nb * out_channels * out_samplesSize;
        break;
    }

    return dataSize;
}
