# Frida 静态库依赖说明

由于静态库过于庞大，本项目**不直接提供**编译好的静态库文件。

请按照以下步骤手动准备依赖环境：

1. 前往 Frida 官方 GitHub 下载页面：
  https://github.com/frida/frida/releases/download/16.7.14/frida-gumjs-devkit-16.7.14-android-arm64.tar.xz
 如果需要自行寻找，可参考名称
 frida-gumjs-devkit-16.7.14-android-arm64.tar.xz

2. 解压该文件，将其中的以下三个文件：
   - `frida-gumjs.h` (头文件)
   - `libfrida-gumjs.a` (静态库文件)
   - `frida-gumjs-example.c`
   
  复制到当前目录 `Frida/arm64-v8a/` 下
   
3. 准备完成后，即可回到项目根目录进行 CMake 编译。
