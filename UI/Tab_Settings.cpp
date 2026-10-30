#include "Tabs.h"
#include "UIManager.h"
#include "KeyboardBridge.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

extern bool g_touch_penetration;
extern bool g_hide_injection_features;
extern float g_fab_x;
extern float g_fab_y;
extern const char* GetLogFilePath();

namespace Tabs {
    static const char* s_fps_limits[] = { "无限制", "120 FPS", "90 FPS", "60 FPS", "30 FPS", "15 FPS", "5 FPS" };

    void ApplyTheme(int idx) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark();

        if (idx == 0) { 
            style.Colors[ImGuiCol_Button]        = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
        } 
        else if (idx == 1) { 
            ImGui::StyleColorsLight();
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 0.96f);
            style.Colors[ImGuiCol_FrameBg]  = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
            style.Colors[ImGuiCol_TitleBg]  = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
            style.Colors[ImGuiCol_Button]   = ImVec4(0.60f, 0.60f, 0.60f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.70f, 0.70f, 0.90f);
            style.Colors[ImGuiCol_Header]   = ImVec4(0.70f, 0.70f, 0.70f, 0.80f);
            style.Colors[ImGuiCol_Tab]      = ImVec4(0.80f, 0.80f, 0.80f, 0.80f);
            style.Colors[ImGuiCol_TabActive]= ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        }
        else if (idx == 2) { 
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.06f, 0.09f, 0.16f, 0.96f);
            style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.04f, 0.07f, 0.12f, 0.96f);
            style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.06f, 0.09f, 0.16f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.10f, 0.15f, 0.25f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.25f, 0.45f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.15f, 0.20f, 0.30f, 0.80f);
            style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.20f, 0.30f, 0.45f, 0.80f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.35f, 0.60f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.75f, 0.90f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.35f, 0.60f, 0.50f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.45f, 0.75f, 0.80f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.12f, 0.20f, 0.35f, 0.80f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.20f, 0.35f, 0.60f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);
        }
        else if (idx == 3) { 
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.10f, 0.06f, 0.14f, 0.96f);
            style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.08f, 0.04f, 0.10f, 0.96f);
            style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.10f, 0.06f, 0.14f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.16f, 0.10f, 0.22f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.15f, 0.40f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.22f, 0.15f, 0.28f, 0.80f);
            style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.30f, 0.20f, 0.38f, 0.80f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.40f, 0.20f, 0.50f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.30f, 0.65f, 0.90f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.40f, 0.20f, 0.50f, 0.50f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.30f, 0.65f, 0.80f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.20f, 0.12f, 0.28f, 0.80f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.40f, 0.20f, 0.50f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(0.80f, 0.40f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.60f, 0.30f, 0.80f, 1.00f);
        }
        else if (idx == 4) { 
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.04f, 0.08f, 0.06f, 0.96f);
            style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.02f, 0.06f, 0.04f, 0.96f);
            style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.04f, 0.08f, 0.06f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.08f, 0.15f, 0.10f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.35f, 0.20f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.10f, 0.22f, 0.15f, 0.80f);
            style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.15f, 0.30f, 0.20f, 0.80f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.50f, 0.30f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.65f, 0.40f, 0.90f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.50f, 0.30f, 0.50f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.65f, 0.40f, 0.80f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.10f, 0.25f, 0.15f, 0.80f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.20f, 0.50f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(0.30f, 1.00f, 0.50f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.20f, 0.70f, 0.40f, 1.00f);
        }
        else if (idx == 5) { 
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.10f, 0.05f, 0.06f, 0.96f);
            style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.08f, 0.03f, 0.04f, 0.96f);
            style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.10f, 0.05f, 0.06f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.18f, 0.08f, 0.10f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.40f, 0.15f, 0.20f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.25f, 0.12f, 0.15f, 0.80f);
            style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.35f, 0.18f, 0.22f, 0.80f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.50f, 0.20f, 0.25f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.65f, 0.30f, 0.35f, 0.90f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.50f, 0.20f, 0.25f, 0.50f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.65f, 0.30f, 0.35f, 0.80f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.25f, 0.10f, 0.12f, 0.80f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.50f, 0.20f, 0.25f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(1.00f, 0.40f, 0.40f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.80f, 0.30f, 0.30f, 1.00f);
        }
        else if (idx == 6) { 
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.05f, 0.10f, 0.12f, 0.96f);
            style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.03f, 0.08f, 0.10f, 0.96f);
            style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.05f, 0.10f, 0.12f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.08f, 0.18f, 0.22f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.40f, 0.45f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.10f, 0.25f, 0.30f, 0.80f);
            style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.15f, 0.35f, 0.40f, 0.80f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.55f, 0.60f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.70f, 0.75f, 0.90f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.55f, 0.60f, 0.50f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.70f, 0.75f, 0.80f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.10f, 0.25f, 0.28f, 0.80f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.20f, 0.55f, 0.60f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(0.40f, 1.00f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.30f, 0.80f, 0.80f, 1.00f);
        }
        else if (idx == 7) { 
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.12f, 0.12f, 0.12f, 0.96f);
            style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.96f);
            style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.12f, 0.12f, 0.12f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.80f);
            style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.28f, 0.28f, 0.28f, 0.80f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.90f);
            style.Colors[ImGuiCol_Header]        = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.80f);
            style.Colors[ImGuiCol_Tab]           = ImVec4(0.15f, 0.15f, 0.15f, 0.80f);
            style.Colors[ImGuiCol_TabActive]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        }
        else if (idx == 8) { 
            ImGui::StyleColorsLight();
            style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.92f, 0.92f, 0.92f, 0.98f);
            style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
            style.Colors[ImGuiCol_Button]        = ImVec4(0.75f, 0.75f, 0.75f, 0.80f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
            style.Colors[ImGuiCol_CheckMark]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        }
    }

    void DrawSettingsTab() {
        const char* themes[] = { "默认暗黑", "明亮模式", "深邃蓝", "暗夜紫", "赛博绿", "樱桃红", "海洋青", "极简黑白", "极简灰白" };
        ImGui::Text("主题色");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##theme", &s_themeIdx, themes, IM_ARRAYSIZE(themes))) {
            ApplyTheme(s_themeIdx);
        }

        ImGui::Spacing();
        ImGui::Text("UI 缩放：%.2f", s_uiScale);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##uiscale", &s_uiScale, 0.5f, 2.5f)) { 
            ImGui::GetIO().FontGlobalScale = s_uiScale; 
        }
        
        ImGui::Spacing(); 

        if (ImGui::Checkbox("小悬浮球触摸穿透", &s_touch_penetration)) {
            g_touch_penetration = s_touch_penetration;
        }
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("开启后，点击悬浮球也会点到底下的游戏；\n关闭后，点击悬浮球完全不会影响游戏。");
        }

        ImGui::Checkbox("调试 FPS", &s_showFps);
        if (s_showFps) {
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##fps_limit", &s_fps_limit_idx, s_fps_limits, IM_ARRAYSIZE(s_fps_limits));
        }
        
        ImGui::Checkbox("防扫描模式[测试]", &g_hide_injection_features);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("开启后启动，库，dex防扫描，需重启生效");
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::CollapsingHeader("界面标签页显示设置", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("脚本页", &s_show_tab_script);
            ImGui::SameLine();
            ImGui::Checkbox("模板页", &s_show_tab_template);
            ImGui::SameLine();
            ImGui::Checkbox("管理页", &s_show_tab_manage);
            
            ImGui::Checkbox("日志页", &s_show_tab_log);
            ImGui::SameLine();
            ImGui::Checkbox("预设页", &s_show_tab_presets);
            
            bool true_val = true;
            ImGui::BeginDisabled();
            ImGui::Checkbox("设置页(常驻)", &true_val);
            ImGui::EndDisabled();
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::Text("默认 So 名称");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##so_name", s_default_so_name, sizeof(s_default_so_name), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_default_so_name);
        if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_default_so_name, sizeof(s_default_so_name)); KeyboardBridge::Show(); }
        if (ImGui::IsItemDeactivated()) { KeyboardBridge::Hide(); }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::CollapsingHeader("自动化控制", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("定时卸载全部脚本", &s_auto_stop_enable);
            if (s_auto_stop_enable) {
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("##auto_stop_int", &s_auto_stop_interval, 1, 60, "间隔 %d 秒");
                
                float remaining = s_auto_stop_interval - (float)(ImGui::GetTime() - s_auto_stop_timer);
                if (remaining < 0) remaining = 0;
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "距离下次卸载还有: %.1f 秒", remaining);
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::TextDisabled("配置管理");
        if (ImGui::Button("保存当前设置与脚本", ImVec2(-1, 50))) {
            PerformSilentSave(); 
            ImGui::OpenPopup("保存成功");
        }
        
        if (ImGui::Button("删除所有配置", ImVec2(-1, 50))) {
            std::string logPath = GetLogFilePath();
            if (!logPath.empty()) {
                size_t pos = logPath.find_last_of('/');
                if (pos != std::string::npos) {
                    std::string dir = logPath.substr(0, pos);
                    std::remove((dir + "/modmenu_config.ini").c_str());
                    std::remove((dir + "/modmenu_script.js").c_str());
                    std::remove((dir + "/modmenu_templates.ini").c_str());
                }
            }
            
            s_uiScale = 1.0f;
            ImGui::GetIO().FontGlobalScale = 1.0f;
            s_showFps = false;
            s_fps_limit_idx = 0;
            s_themeIdx = 2; 
            ApplyTheme(2);
            
            s_touch_penetration = false;
            g_touch_penetration = false;
            g_hide_injection_features = false;
            s_custom_templates.clear();
            
            s_show_tab_script = true;
            s_show_tab_template = true;
            s_show_tab_manage = true;
            s_show_tab_settings = true;
            s_show_tab_log = true;
            s_show_tab_presets = true;
            snprintf(s_default_so_name, sizeof(s_default_so_name), "libil2cpp.so");
            
            s_auto_stop_enable = false;
            s_auto_stop_interval = 5;

            g_fab_x = -1; 
            g_fab_y = -1;
            snprintf(s_scriptBuf, sizeof(s_scriptBuf), "console.log('Hello Frida GumJS!');\n");

            snprintf(s_nc_offset, sizeof(s_nc_offset), "0x0");
            snprintf(s_nc_hook_offset, sizeof(s_nc_hook_offset), "");
            s_nc_arg_count = 1;
            snprintf(s_nc_arg_count_str, sizeof(s_nc_arg_count_str), "1");
            snprintf(s_nc_arg_content[0], sizeof(s_nc_arg_content[0]), "0x0");
            s_nc_arg_type[0] = 2;
            snprintf(s_nc_call_count_str, sizeof(s_nc_call_count_str), "1");
            snprintf(s_nc_interval_str, sizeof(s_nc_interval_str), "10");
            
            ImGui::OpenPopup("删除成功");
        }

        if (ImGui::BeginPopupModal("保存成功", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("配置、脚本及自定义模板已成功保存！"); ImGui::Spacing();
            if (ImGui::Button("确定", ImVec2(-1, 40))) { ImGui::CloseCurrentPopup(); } ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("删除成功", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("所有配置、脚本及模板已清空，恢复默认！"); ImGui::Spacing();
            if (ImGui::Button("确定", ImVec2(-1, 40))) { ImGui::CloseCurrentPopup(); } ImGui::EndPopup();
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::Text("分辨率：%d x %d", UIManager::GetWidth(), UIManager::GetHeight());
        ImGui::Text("ImGui 版本：%s", IMGUI_VERSION);
        ImGui::Text("状态：运行中");
        if (s_showFps) { ImGui::Spacing(); ImGui::Text("FPS：%.1f", ImGui::GetIO().Framerate); }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextDisabled("--- 鸣谢 (Open Source Projects) ---");
        ImGui::BulletText("Dear ImGui - 强大的 C++ 即时渲染 UI 库");
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "  https://github.com/ocornut/imgui");
        ImGui::BulletText("Frida - 动态插桩工具包 (GumJS 引擎)");
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "  https://github.com/frida/frida");
        ImGui::BulletText("Android NDK & OpenGL ES");
        ImGui::Spacing();
        ImGui::TextWrapped("感谢上述开源项目及其贡献者，本项目基于它们才得以实现。");
    }
}