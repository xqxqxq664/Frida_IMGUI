cmake_minimum_required(VERSION 3.18.1)

set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM 24)
set(ANDROID_STL c++_static)
set(CMAKE_BUILD_TYPE Release)

project(Frida_IMGUI CXX C ASM)
set(CMAKE_CXX_STANDARD 17)

add_library(frida-gumjs STATIC IMPORTED)
set_target_properties(frida-gumjs PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/Frida/arm64-v8a/libfrida-gumjs.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/Frida/arm64-v8a
)

set(SOURCES
    main.cpp
    script.cpp
    assets/dex_data.S
    UI/UIManager.cpp
    UI/InputHandler.cpp
    UI/Tabs.cpp
    UI/KeyboardBridge.cpp
    ImGui/imgui.cpp
    ImGui/imgui_draw.cpp
    ImGui/imgui_tables.cpp
    ImGui/imgui_widgets.cpp
    ImGui/imgui_impl_opengl3.cpp
    ImGui/imgui_impl_android.cpp
)

add_library(Frida_IMGUI SHARED ${SOURCES})

add_custom_command(TARGET Frida_IMGUI POST_BUILD
    COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:Frida_IMGUI>
)

target_include_directories(Frida_IMGUI PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Frida/arm64-v8a
    ${CMAKE_SOURCE_DIR}/ImGui
    ${CMAKE_SOURCE_DIR}/UI
    ${CMAKE_SOURCE_DIR}/assets
)

target_link_libraries(Frida_IMGUI PRIVATE
    -Wl,--whole-archive
    frida-gumjs
    -Wl,--no-whole-archive
    log
    dl
    GLESv3
    EGL
    android
    z
    m
)