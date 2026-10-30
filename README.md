[English](README_EN.md) | 简体中文

# Frida_IMGUI

## 1. 项目介绍

Frida_IMGUI 是一个完全在安卓端运行的Frida控制台，基于ImGui支持可视化动态管理脚本，在软件内部渲染ImGui，无需电脑端参与。

其主要目的是为了解决传统 Frida 必须依赖 PC 端数据线与常驻 `frida-server` ，和没有好看的UI的问题，提供一个无需连接电脑、直接在移动端进行动态调试、代码注入与内存分析的集成环境。

### 🖼️ 界面预览

![主菜单界面](https://github.com/user-attachments/assets/7fbf3d5d-86ef-423d-acfc-04328f8994bd)

![运行日志界面](https://github.com/user-attachments/assets/9649c6e3-e131-4de7-9484-e9c0aa6005f5)

## 2. 项目优势

- **使用简便**：摆脱了独立进程与默认端口，无需电脑。通过重打包（免 Root）或 动态注入。
- **图形化交互与多脚本管理**：内置防穿透与拖拽机制的悬浮窗，支持小悬浮窗使大悬浮窗自由开关，支持多脚本并发运行与一键卸载，并提供了可持久化保存的脚本模板库。
- **全栈 Hook 能力**：底层适配 Frida 16.7.14。既可对普通 Android 应用的 Java 层进行 Hook（`Java.perform`），Native 层的 `.so` 库进行拦截与主动调用（`Interceptor`）。
- **脚本配置保存机制**：提供了保存配置按钮，并且自动静默保存，下次注入同一目标时自动应用
- **线程名更改机制**：
  - 内置线程更改：初始化时自动遍历并重命名 `gum-js-loop` 等线程，重命名为自定义名称，便于日志区分与排查。
- **预设脚本库**：提供了一些预设Frida js脚本(主动调用native函数，il2cpp dump)，且支持UI。


## 3. 使用说明

本项目提供两种部署与使用方案：静态打包（免 Root） 与 Root 注入。

### 方案 A：静态打包（适用于无 Root 环境）

通过修改原版 APK，将 `.so` 静态打包进应用中，实现免 Root 运行。

1. 使用各类 APK 反编译/重打包工具（如 Apktool 等）解包目标 APK。
2. 将编译好的 `libFrida_IMGUI.so` 放入解包目录的 `lib/arm64-v8a/` 文件夹中。
3. 找到目标应用的入口 Activity（或自定义 Application），在 `onCreate` 或 `attachBaseContext` 方法对应的 Smali 代码中插入加载逻辑：

```smali
const-string v0, "Frida_IMGUI"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```

4. 重新打包 APK。应用启动后，悬浮窗将自动加载。

方案 B：动态注入（适用于有 Root 权限环境）

无需修改目标 APK，利用动态注入加载项目。

1. 注入工具：使用各类动态注入工具，将 libFrida_IMGUI.so 加载到目标进程。
   (注：若上述注入方式受限于系统环境或触发保护机制，可自行选用其他符合要求的提权注入方案。)

兼容性提示:
兼容性与测试环境
本项目目前在少数设备上实测通过。
目前仅提供 arm64-v8a 架构的编译支持。
由于 Android 碎片化严重，不同厂商的 SELinux 策略和后台机制可能导致运行失败（如闪退、加载不出悬浮窗）。本项目无法保证在所有设备上完美运行，请在测试前做好数据备份。

4. 编译指南

环境要求

· CMake 3.18.1 及以上
· 目标架构：arm64-v8a

依赖说明

编译前请务必确认Frida/arm64-v8a/已部署好16.7.14 Frida静态库 否则无法正常运行!
具体操作请见Frida/arm64-v8a/README.md

编译步骤

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-24 \
      ..
make -j8
```

5. 鸣谢项目

本项目的实现离不开以下优秀的开源项目：

· Dear ImGui：提供了极其强大的即时渲染 UI 框架。
· Frida：提供了顶级的动态插桩技术与 GumJS 脚本引擎。

本项目为独立第三方项目，与 Frida 官方无隶属或背书关系；Frida 名称仅用于描述所依赖的技术。


本项目基于 MIT License 协议开源。