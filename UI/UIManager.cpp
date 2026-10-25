#include "UIManager.h"
#include "Tabs.h"
#include "KeyboardBridge.h"

#include <GLES3/gl3.h>
#include <cmath>
#include <chrono>
#include <unistd.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_android.h"
#include "font.h"

extern float g_fab_x;
extern float g_fab_y;

namespace UIManager {

static bool  g_ready    = false;
static int   g_w        = 1080;
static int   g_h        = 2360;
static bool  g_showMenu = true;
static GLuint g_fab_texture = 0; 

static bool   g_is_dragging = false;
static ImVec2 g_drag_offset;
static ImVec2 g_mouse_down_pos;

static void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding   = ImVec2(12, 10);
    style.ItemSpacing    = ImVec2(14, 14);
    style.WindowPadding  = ImVec2(16, 16);
    style.WindowRounding    = 16.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 12.0f;
    style.PopupRounding     = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 12.0f;
    style.TabRounding       = 10.0f;
}

void Init() {
    if (g_ready) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    io.Fonts->AddFontFromMemoryTTF((void*)font_v, font_v_size, 28.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
    ImGui::StyleColorsDark();
    ApplyStyle();
    ImGui_ImplOpenGL3_Init("#version 300 es");
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    g_fab_texture = 0;
    g_ready = true;
}

void OnSurfaceChanged(int w, int h) {
    g_w = w; g_h = h;
}

void OnFrame() {
    if (Tabs::s_fps_limit_idx > 0) {
        int target_fps = 0;
        switch (Tabs::s_fps_limit_idx) {
            case 1: target_fps = 120; break;
            case 2: target_fps = 90;  break;
            case 3: target_fps = 60;  break;
            case 4: target_fps = 30;  break;
            case 5: target_fps = 15;  break;
            case 6: target_fps = 5;   break;
            default: target_fps = 0;  break;
        }
        
        if (target_fps > 0) {
            static auto last_time = std::chrono::high_resolution_clock::now();
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = current_time - last_time;
            double target_dt = 1.0 / target_fps;
            
            if (elapsed.count() < target_dt) {
                int sleep_us = (int)((target_dt - elapsed.count()) * 1000000);
                if (sleep_us > 0) usleep(sleep_us);
            }
            last_time = std::chrono::high_resolution_clock::now();
        }
    }

    if (!g_ready) return;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_w, (float)g_h);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(g_w, g_h);
    ImGui::NewFrame();

    float fab_size = 110.0f; 
    if (g_fab_x < 0 || g_fab_y < 0) {
        g_fab_x = g_w - fab_size - 50.0f;
        g_fab_y = 100.0f;
    }

    ImVec2 mouse_pos = io.MousePos;
    bool is_hovered = (mouse_pos.x >= g_fab_x && mouse_pos.x <= g_fab_x + fab_size && 
                       mouse_pos.y >= g_fab_y && mouse_pos.y <= g_fab_y + fab_size);

    if (!io.MouseDown[0]) {
        if (g_is_dragging) {
            float dx = mouse_pos.x - g_mouse_down_pos.x;
            float dy = mouse_pos.y - g_mouse_down_pos.y;
            if (dx * dx + dy * dy < 25.0f) {
                g_showMenu = !g_showMenu;
            }
        }
        g_is_dragging = false; 
    }

    if (is_hovered && ImGui::IsMouseClicked(0)) {
        g_is_dragging = true;
        g_drag_offset = ImVec2(mouse_pos.x - g_fab_x, mouse_pos.y - g_fab_y);
        g_mouse_down_pos = mouse_pos;
    }

    if (g_is_dragging && io.MouseDown[0]) {
        g_fab_x = mouse_pos.x - g_drag_offset.x;
        g_fab_y = mouse_pos.y - g_drag_offset.y;
        
        if (g_fab_x < 0) g_fab_x = 0;
        if (g_fab_y < 0) g_fab_y = 0;
        if (g_fab_x > g_w - fab_size) g_fab_x = g_w - fab_size;
        if (g_fab_y > g_h - fab_size) g_fab_y = g_h - fab_size;
    }

    if (g_showMenu) {
        float short_side = (g_w < g_h) ? (float)g_w : (float)g_h;
        float size = short_side * 0.85f; 
        float x = (g_w - size) * 0.5f;
        float y = (g_h - size) * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(x, y),  ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(size, size), ImGuiCond_FirstUseEver);
     //   ImGui::SetNextWindowSizeConstraints(ImVec2(short_side * 0.4f, short_side * 0.4f), ImVec2(g_w * 1.0f, g_h * 0.95f));

        ImGuiWindowFlags menu_flags = 0;
        
        if (is_hovered || g_is_dragging) {
            menu_flags |= ImGuiWindowFlags_NoMove; 
        }

        ImGui::Begin("Frida_IMGUI", &g_showMenu, menu_flags);
        Tabs::DrawAll();
        ImGui::End();
    }

    {
        ImVec2 fab_pos = ImVec2(g_fab_x, g_fab_y);
        ImVec2 center = ImVec2(fab_pos.x + fab_size * 0.5f, fab_pos.y + fab_size * 0.5f);
        float radius = fab_size * 0.5f;

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        
        drawList->AddCircleFilled(ImVec2(center.x + 3, center.y + 3), radius, IM_COL32(0, 0, 0, 100), 32);
        drawList->AddCircleFilled(center, radius, IM_COL32(30, 60, 120, 255), 32);
        drawList->AddCircle(center, radius, IM_COL32(100, 150, 255, 200), 32, 2.5f);
        
        ImU32 textColor = IM_COL32(255, 255, 255, 255);
        float thick = radius * 0.12f;
        float height = radius * 0.9f;
        float width = radius * 0.5f;
        
        float startX = center.x - width * 0.5f; 
        float startY = center.y - height * 0.5f;
        
        drawList->AddLine(ImVec2(startX, startY), ImVec2(startX, startY + height), textColor, thick);
        drawList->AddLine(ImVec2(startX, startY), ImVec2(startX + width, startY), textColor, thick);
        drawList->AddLine(ImVec2(startX, startY + height * 0.5f), ImVec2(startX + width * 0.8f, startY + height * 0.5f), textColor, thick);
    }

    KeyboardBridge::Tick();
    ImGui::EndFrame();

    glViewport(0, 0, g_w, g_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool IsReady()   { return g_ready; }
int  GetWidth()  { return g_w; }
int  GetHeight() { return g_h; }

}