#pragma once

namespace InputHandler {
    // 返回 true 表示 ImGui 消费了这次触摸，游戏不应再收到
    bool Handle(int action, float x, float y);
}