#pragma once

namespace UIManager {
    void Init();
    void OnSurfaceChanged(int w, int h);
    void OnFrame();
    bool IsReady();
    int  GetWidth();
    int  GetHeight();
}