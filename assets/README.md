如何修改并更新挂载用的 Dex（MyRenderer.java）？
由于 C++ 编译无法直接包含 Java 文件，本项目通过 assets/dex_data.S 将 classes.dex 静态打包进 .so 中。如果你修改了 java_src/MyRenderer.java，请按照以下步骤重新生成：

1. 使用 Android Studio 或 javac 编译该 Java 文件。
2. 使用 d8 工具（位于 Android SDK 的 build-tools 目录）将 .class 文件转换为 classes.dex。(编译器编译classes.dex)
3. 将生成的 classes.dex 替换 assets/ 目录下的同名文件。
4. 重新编译项目（dex_data.S 会自动将最新的 dex 打包进去）。