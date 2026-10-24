#include "KeyboardBridge.h"
#include <cstring>
#include <string>
#include <mutex>

namespace KeyboardBridge {

static char*    g_buf     = nullptr;
static int      g_bufSize = 0;

static std::mutex   g_mtx;
static std::string  g_pendingText;
static bool         g_hasPending = false;

static bool         g_shown = false;

static JavaVM*      g_vm = nullptr;
static jclass       g_rendererCls = nullptr;
static jmethodID    g_showMid = nullptr;
static jmethodID    g_hideMid = nullptr;

void Init(JNIEnv* env, jclass rendererCls, JavaVM* vm) {
    g_vm = vm;
    g_rendererCls = (jclass)env->NewGlobalRef(rendererCls);

    g_showMid = env->GetStaticMethodID(rendererCls, "showKeyboard", "(Ljava/lang/String;)V");
    g_hideMid = env->GetStaticMethodID(rendererCls, "hideKeyboard", "()V");

    if (!g_showMid) env->ExceptionClear();
    if (!g_hideMid) env->ExceptionClear();
}

void SetActiveBuffer(char* buf, int size) {
    g_buf = buf;
    g_bufSize = size;
}

static void CallJavaShow(const std::string& text) {
    if (!g_showMid || !g_rendererCls || !g_vm) return;

    JNIEnv* env = nullptr;
    bool needDetach = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        needDetach = true;
    }

    jstring jText = env->NewStringUTF(text.c_str());
    env->CallStaticVoidMethod(g_rendererCls, g_showMid, jText);
    env->DeleteLocalRef(jText);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (needDetach) g_vm->DetachCurrentThread();
}

static void CallJavaHide() {
    if (!g_hideMid || !g_rendererCls || !g_vm) return;

    JNIEnv* env = nullptr;
    bool needDetach = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        needDetach = true;
    }

    env->CallStaticVoidMethod(g_rendererCls, g_hideMid);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (needDetach) g_vm->DetachCurrentThread();
}

void Show() {
    if (!g_buf) return;
    if (g_shown) return;
    CallJavaShow(std::string(g_buf));
    g_shown = true;
}

void Hide() {
    if (!g_shown) return;
    CallJavaHide();
    g_shown = false;
}

void Tick() {}

void OnImeText(const char* text) {
    if (!text) return;

    std::string t = text;
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();

    std::lock_guard<std::mutex> lock(g_mtx);
    g_pendingText = t;
    g_hasPending = true;
}

bool HasPending() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_hasPending;
}

std::string TakePending() {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_hasPending = false;
    std::string ret = g_pendingText;
    g_pendingText.clear();
    return ret;
}

bool IsShown() { return g_shown; }

void OnJavaKeyboardClosed() {
    g_shown = false;
}

}