#include <jni.h>
#include <memory>
#include "PadEngine.h"

static std::unique_ptr<PadEngine> g_engine;

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeInit(JNIEnv*, jobject) {
    g_engine = std::make_unique<PadEngine>();
    g_engine->start();
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeLoadSample(JNIEnv *env, jobject, jint preset, jint pad, jshortArray pcm, jint channels) {
    if (!g_engine) return;
    jsize len = env->GetArrayLength(pcm);
    jshort* data = env->GetShortArrayElements(pcm, nullptr);
    int numFrames = len / (channels > 0 ? channels : 1);
    g_engine->loadSample(preset, pad, reinterpret_cast<int16_t*>(data), numFrames, channels);
    env->ReleaseShortArrayElements(pcm, data, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeClearPad(JNIEnv*, jobject, jint preset, jint pad) {
    if (g_engine) g_engine->clearPad(preset, pad);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeTrigger(JNIEnv*, jobject, jint preset, jint pad) {
    if (g_engine) g_engine->trigger(preset, pad);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeShutdown(JNIEnv*, jobject) {
    if (g_engine) {
        g_engine->stop();
        g_engine.reset();
    }
}

// Dipanggil dari onPause/onResume Activity (BUKAN onCreate/onDestroy). Ini yang
// bikin app langsung ngelepas jalur audio exclusive begitu pindah ke background,
// jadi kalau user buka app satunya (mis. dari FREE ke Premium atau sebaliknya),
// app yang baru dibuka bisa dapet jalur exclusive juga -> bukan kepaksa jatuh ke
// mode Shared (yang keknya lebih keras & delay) gara-gara app sebelumnya masih
// "nyekek" hardware audio walau udah gak keliatan di layar.
// Beda sama nativeShutdown(): stop/start di sini gak nyentuh g_engine itu sendiri,
// jadi semua sample custom yang udah di-load ke pad gak perlu di-load ulang pas
// balik lagi ke app-nya.
extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativePause(JNIEnv*, jobject) {
    // Kalau lagi rekam, jangan dilepas -> lebih penting jaga rekaman gak keputus
    // daripada ngalah ke app lain yang minta jalur exclusive.
    if (g_engine && !g_engine->isRecording()) {
        g_engine->stop();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeResume(JNIEnv*, jobject) {
    if (g_engine) g_engine->start();
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeStartRecording(JNIEnv*, jobject) {
    if (g_engine) g_engine->startRecording();
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_com_kendang_realpads_AudioEngine_nativeStopRecording(JNIEnv *env, jobject) {
    if (!g_engine) return env->NewShortArray(0);
    std::vector<int16_t> pcm = g_engine->stopRecording();
    jshortArray result = env->NewShortArray((jsize)pcm.size());
    if (result != nullptr && !pcm.empty()) {
        env->SetShortArrayRegion(result, 0, (jsize)pcm.size(), reinterpret_cast<jshort*>(pcm.data()));
    }
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kendang_realpads_AudioEngine_nativeLoadMusic(JNIEnv *env, jobject, jshortArray pcm, jint channels) {
    if (!g_engine || pcm == nullptr) return JNI_FALSE;
    jsize len = env->GetArrayLength(pcm);
    if (len <= 0) return JNI_FALSE;
    jshort* data = env->GetShortArrayElements(pcm, nullptr);
    if (data == nullptr) return JNI_FALSE; // JNI gagal alokasi buffer -> gagal dengan rapi, bukan crash
    int numFrames = len / (channels > 0 ? channels : 1);
    bool ok = g_engine->loadMusic(reinterpret_cast<int16_t*>(data), numFrames, channels);
    env->ReleaseShortArrayElements(pcm, data, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativePlayMusic(JNIEnv*, jobject) {
    if (g_engine) g_engine->playMusic();
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeStopMusic(JNIEnv*, jobject) {
    if (g_engine) g_engine->stopMusic();
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSeekMusic(JNIEnv*, jobject, jdouble seconds) {
    if (g_engine) g_engine->seekMusic(seconds);
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetMusicPosition(JNIEnv*, jobject) {
    return g_engine ? g_engine->getMusicPositionSeconds() : 0.0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetMusicDuration(JNIEnv*, jobject) {
    return g_engine ? g_engine->getMusicDurationSeconds() : 0.0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kendang_realpads_AudioEngine_nativeIsMusicPlaying(JNIEnv*, jobject) {
    return g_engine ? (jboolean)g_engine->isMusicPlaying() : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetKendangVolume(JNIEnv*, jobject, jfloat v) {
    if (g_engine) g_engine->setKendangVolume(v);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetMusicVolume(JNIEnv*, jobject, jfloat v) {
    if (g_engine) g_engine->setMusicVolume(v);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadVolume(JNIEnv*, jobject, jint preset, jint pad, jfloat v) {
    if (g_engine) g_engine->setPadVolume(preset, pad, v);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadVolume(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? g_engine->getPadVolume(preset, pad) : 1.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadTune(JNIEnv*, jobject, jint preset, jint pad, jfloat semitones) {
    if (g_engine) g_engine->setPadTune(preset, pad, semitones);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadTune(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? g_engine->getPadTune(preset, pad) : 0.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadReverb(JNIEnv*, jobject, jint preset, jint pad, jfloat amount) {
    if (g_engine) g_engine->setPadReverb(preset, pad, amount);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadReverb(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? g_engine->getPadReverb(preset, pad) : 0.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadChoke(JNIEnv*, jobject, jint preset, jint pad, jboolean enabled) {
    if (g_engine) g_engine->setPadChoke(preset, pad, enabled == JNI_TRUE);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadChoke(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? (jboolean)(g_engine->getPadChoke(preset, pad) ? JNI_TRUE : JNI_FALSE) : JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadCrossChokeEnabled(JNIEnv*, jobject, jint preset, jint pad, jboolean enabled) {
    if (g_engine) g_engine->setPadCrossChokeEnabled(preset, pad, enabled == JNI_TRUE);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadCrossChokeEnabled(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? (jboolean)(g_engine->getPadCrossChokeEnabled(preset, pad) ? JNI_TRUE : JNI_FALSE) : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadCrossChokeMask(JNIEnv*, jobject, jint preset, jint pad, jint mask) {
    if (g_engine) g_engine->setPadCrossChokeMask(preset, pad, mask);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadCrossChokeMask(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? (jint)g_engine->getPadCrossChokeMask(preset, pad) : 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadLoop(JNIEnv*, jobject, jint preset, jint pad, jboolean enabled) {
    if (g_engine) g_engine->setPadLoop(preset, pad, enabled == JNI_TRUE);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadLoop(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? (jboolean)(g_engine->getPadLoop(preset, pad) ? JNI_TRUE : JNI_FALSE) : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_kendang_realpads_AudioEngine_nativeSetPadLoopTempo(JNIEnv*, jobject, jint preset, jint pad, jfloat rate) {
    if (g_engine) g_engine->setPadLoopTempo(preset, pad, rate);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_kendang_realpads_AudioEngine_nativeGetPadLoopTempo(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? g_engine->getPadLoopTempo(preset, pad) : 1.0f;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kendang_realpads_AudioEngine_nativeIsPadLooping(JNIEnv*, jobject, jint preset, jint pad) {
    return g_engine ? (jboolean)(g_engine->isPadLooping(preset, pad) ? JNI_TRUE : JNI_FALSE) : JNI_FALSE;
}
