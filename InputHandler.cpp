#include "InputHandler.h"
#include "imgui.h"
#include "UIManager.h"

extern float g_fab_x;
extern float g_fab_y;
extern bool g_touch_penetration;

namespace InputHandler {

bool Handle(int action, float x, float y) {
    ImGuiIO& io = ImGui::GetIO();

    // 将触摸事件同步至 ImGui 上下文
    switch (action) {
        case 0: io.AddMousePosEvent(x, y); io.AddMouseButtonEvent(0, true); break;
        case 2: io.AddMousePosEvent(x, y); break;
        case 1: io.AddMousePosEvent(x, y); io.AddMouseButtonEvent(0, false); break;
        case 3: io.AddMouseButtonEvent(0, false); break;
        default: break;
    }

    // 拦截悬浮球区域的触摸，防止拖动时穿透给底层游戏
    float fab_size = 110.0f; 
    if (g_fab_x >= 0 && g_fab_y >= 0) {
        if (x >= g_fab_x && x <= g_fab_x + fab_size && y >= g_fab_y && y <= g_fab_y + fab_size) {
            return true; 
        }
    }

    // 全局穿透模式开启时，除悬浮球外一律放行
    if (g_touch_penetration) {
        return false; 
    }

    // 依赖 ImGui 自身的命中判定（WantCaptureMouse）
    // 大窗口未开启或触摸点在窗口外时返回 false，实现原生的触摸透传
    return io.WantCaptureMouse;
}

}