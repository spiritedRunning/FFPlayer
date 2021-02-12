package com.example.ffplayer;

import android.view.Surface;

public class FFPlayer {

    static {
        System.loadLibrary("native-lib");
    }


    private final long nativeHandle;

    public FFPlayer() {
        nativeHandle = nativeInit();
    }

    public void setDataSource(String path) {
        setDataSource(nativeHandle, path);
    }

    public void prepare() {
        prepare(nativeHandle);
    }

    public void setSurface(Surface surface) {
        setSurface(nativeHandle, surface);
    }

    public void start() {
        start(nativeHandle);
    }

    public void pause() {

    }

    public void stop() {
        stop(nativeHandle);
    }


    private OnErrorListener onErrorListener;
    private OnPrepareListener onPrepareListener;
    private OnProgressListener onProgressListener;

    private void onError(int errCode) {
        if (onErrorListener != null) {
            onErrorListener.onError(errCode);
        }
    }

    private void onPrepare() {
        if (onPrepareListener != null) {
            onPrepareListener.onPrepared();
        }
    }

    private void onProgress(int progress) {
        if (onProgressListener != null) {
            onProgressListener.onProgress(progress);
        }
    }


    public void setOnErrorListener(OnErrorListener onErrorListener) {
        this.onErrorListener = onErrorListener;
    }

    public void setOnPrepareListener(OnPrepareListener onPrepareListener) {
        this.onPrepareListener = onPrepareListener;
    }

    public void setOnProgressListener(OnProgressListener onProgressListener) {
        this.onProgressListener = onProgressListener;
    }


    public interface OnErrorListener {
        void onError(int err);
    }

    public interface OnPrepareListener {
        void onPrepared();
    }

    public interface OnProgressListener {
        void onProgress(int progress);
    }



    private native long nativeInit();

    private native void setDataSource(long nativeHandle, String path);

    private native void prepare(long nativeHandle);

    private native void start(long nativeHandle);

    private native void setSurface(long nativeHandle, Surface surface);

    private native void stop(long nativeHandle);
}
