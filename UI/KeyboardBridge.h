#pragma once
#include <jni.h>
#include <string>

namespace KeyboardBridge {

    void Init(JNIEnv* env, jclass rendererCls, JavaVM* vm);

    // Tabs 里 InputText 激活时调
    void SetActiveBuffer(char* buf, int size);

    // 弹/收键盘（可被 Tabs 主动调）
    void Show();
    void Hide();

    // 每帧 Tick（当前为占位，主要逻辑下沉到 callback）
    void Tick();

    // Java 输入法回调入口
    void OnImeText(const char* text);

    // 是否有待注入的文字（Tabs 的 InputTextCallback 用）
    bool HasPending();
    std::string TakePending();

    // 键盘是否已弹
    bool IsShown();

    // 收到 Java 侧"键盘已关闭"通知
    void OnJavaKeyboardClosed();

} // namespace KeyboardBridge