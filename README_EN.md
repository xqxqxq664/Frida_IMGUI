# Frida_IMGUI

## 1. Project Introduction

Frida_IMGUI is a Frida console running entirely on the Android side, based on ImGui to support visual dynamic script management, rendering ImGui internally within the software, without the need for a PC.

Its main purpose is to solve the problems of traditional Frida having to rely on PC data cables and a resident `frida-server`, as well as the lack of a good-looking UI, providing an integrated environment for dynamic debugging, code injection, and memory analysis directly on mobile devices without connecting to a computer.

### 🖼️ UI Preview

![Main Menu Interface](https://github.com/user-attachments/assets/7fbf3d5d-86ef-423d-acfc-04328f8994bd)

![Running Log Interface](https://github.com/user-attachments/assets/9649c6e3-e131-4de7-9484-e9c0aa6005f5)

## 2. Project Advantages

- **Easy to Use**: Gets rid of independent processes and default ports, no PC needed. Through repackaging (No Root required) or dynamic injection.
- **Graphical Interaction and Multi-Script Management**: Built-in floating window with anti-penetration and drag mechanisms, supports a small floating window to freely toggle the large floating window, supports concurrent running of multiple scripts and one-click uninstallation, and provides a persistent script template library.
- **Full-Stack Hook Capabilities**: Bottom layer adapted to Frida 16.7.14. Can perform Hook on the Java layer of ordinary Android applications (`Java.perform`), and interception and active calls on Native layer `.so` libraries (`Interceptor`).
- **Script Configuration Saving Mechanism**: Provides a save configuration button, and automatically silently saves, automatically applied next time the same target is injected.
- **Thread Name Changing Mechanism**:
  - Built-in thread changing: Automatically traverses and renames threads like `gum-js-loop` during initialization, renamed to custom names for easy log differentiation and troubleshooting.
- **Preset Script Library**: Provides some preset Frida js scripts (active call native functions, il2cpp dump), and supports UI.

## 3. Usage Instructions

This project provides two deployment and usage solutions: Static Packaging (No Root required) and Root Injection.

### Method A: Static Packaging (Suitable for No Root Environment)

By modifying the original APK, statically package the `.so` into the application to achieve No Root running.

1. Use various APK decompilation/repackaging tools (such as Apktool, etc.) to unpack the target APK.
2. Place the compiled `libFrida_IMGUI.so` into the `lib/arm64-v8a/` folder of the unpacked directory.
3. Find the entry Activity (or custom Application) of the target application, and insert the loading logic into the Smali code corresponding to the `onCreate` or `attachBaseContext` method:

```smali
const-string v0, "Frida_IMGUI"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```

4. Repackage the APK. After the application starts, the floating window will load automatically.

Method B: Dynamic Injection (Suitable for Root Permission Environment)

No need to modify the target APK, use dynamic injection to load the project.

1. Injection Tools: Use various dynamic injection tools to load libFrida_IMGUI.so into the target process.
   (Note: If the above injection methods are limited by the system environment or trigger protection mechanisms, you can choose other privilege escalation injection solutions that meet the requirements.)

Compatibility Note:
Compatibility and Test Environment
This project is currently tested and passed on a few devices.
Currently only compilation support for the arm64-v8a architecture is provided.
Due to severe Android fragmentation, SELinux policies and background mechanisms of different manufacturers may cause running failures (such as crashes, failure to load the floating window). This project cannot guarantee perfect operation on all devices, please back up your data before testing.

4. Compilation Guide

Environment Requirements

· CMake 3.18.1 and above
· Target Architecture: arm64-v8a

Dependency Description

Before compiling, please be sure to confirm that the 16.7.14 Frida static library has been deployed in Frida/arm64-v8a/, otherwise it cannot run normally!
For specific operations, please see Frida/arm64-v8a/README.md

Compilation Steps

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-24 \
      ..
make -j8
```

5. Acknowledgements

The implementation of this project cannot be separated from the following excellent open-source projects:

· Dear ImGui: Provides an extremely powerful immediate rendering UI framework.
· Frida: Provides top-level dynamic instrumentation technology and GumJS script engine.

This project is an independent third-party project and has no affiliation or endorsement relationship with Frida officials; the Frida name is only used to describe the technology relied upon.

This project is open-sourced based on the MIT License.