#include <jni.h>
#include <string>
#include "FFPlayer.h"

JavaVM *javaVM = 0;


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVM = vm;
    return JNI_VERSION_1_4;
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_ffplayer_FFPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    FFPlayer *player = new FFPlayer(new JavaCallHelper(javaVM, env, thiz));
    return (jlong) player;
}



extern "C"
JNIEXPORT void JNICALL
Java_com_example_ffplayer_FFPlayer_setDataSource(JNIEnv *env, jobject thiz, jlong native_handle,
                                                 jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);
    FFPlayer *player = reinterpret_cast<FFPlayer *>(native_handle);
    player->setDataSource(path);

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_ffplayer_FFPlayer_prepare(JNIEnv *env, jobject thiz, jlong native_handle) {
    FFPlayer *player = reinterpret_cast<FFPlayer *>(native_handle);
    player->prepare();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_ffplayer_FFPlayer_start(JNIEnv *env, jobject thiz, jlong native_handle) {
    FFPlayer *player = reinterpret_cast<FFPlayer *>(native_handle);
    player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_ffplayer_FFPlayer_setSurface(JNIEnv *env, jobject thiz, jlong native_handle,
                                              jobject surface) {
    FFPlayer *player = reinterpret_cast<FFPlayer *>(native_handle);
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    player->setWindow(window);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_ffplayer_FFPlayer_stop(JNIEnv *env, jobject thiz, jlong native_handle) {
    FFPlayer *player = reinterpret_cast<FFPlayer *>(native_handle);
    player->stop();
    delete player;
}