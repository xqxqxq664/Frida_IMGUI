#include "Tabs.h"
#include "UIManager.h"
#include "KeyboardBridge.h"

#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <utility>

extern void WriteLogToFile(const char* logContent);
extern void CopyLogToClipboard(const char* logContent);
extern const char* GetLogFilePath();
extern void SaveConfigToFile(const char* content);
extern void SaveScriptToFile(const char* content);
extern const char* GetConfigCache();
extern const char* GetScriptCache();

extern bool IsJsEngineReady();
extern int GetJsLogCount();
extern void ClearJsLogs();
extern std::deque<std::string> DrainJsLogs();
extern void ExecuteJs(const std::string& code, const std::string& name = "");

extern std::vector<std::pair<int, std::string>> GetActiveScripts();
extern void StopScript(int id);
extern void StopAllScripts();

extern float g_fab_x;
extern float g_fab_y;
extern bool g_touch_penetration;
extern bool g_hide_injection_features;

#include "imgui.h"
#include "imgui_internal.h"

namespace Tabs {

// 标签页开关变量
static bool s_show_tab_script = true;
static bool s_show_tab_template = true;
static bool s_show_tab_manage = true;
static bool s_show_tab_settings = true; // 强制常驻
static bool s_show_tab_log = true;

// 自动卸载配置
static bool s_auto_stop_enable = false;
static int s_auto_stop_interval = 5; // 默认5秒
static double s_auto_stop_timer = 0.0;

static char s_scriptBuf[16384] = R"raw(console.log("Frida版本: " + Frida.version);
console.log("目标架构: " + Process.arch);

if (Java.available) {
    Java.perform(function() {
        var app = Java.use('android.app.ActivityThread').currentApplication();
        var pkg = app ? app.getPackageName() : "未获取到";
        console.log("目标包名: " + pkg);
    });
})raw";

static std::deque<std::string> s_visibleLogs;
float s_uiScale = 1.0f;
bool  s_showFps = false; 
int   s_fps_limit_idx = 0;
static const char* s_fps_limits[] = { "无限制", "120 FPS", "90 FPS", "60 FPS", "30 FPS", "15 FPS", "5 FPS" };
static int   s_themeIdx = 2;    
static bool  s_touch_penetration = false;

static std::vector<std::pair<std::string, std::string>> s_custom_templates;
static char  s_new_template_name[64] = ""; 
static int   s_delete_template_idx = -1; 

static void ApplyTheme(int idx) {
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

static int InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (!KeyboardBridge::HasPending()) return 0;
    std::string text = KeyboardBridge::TakePending();
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text.c_str());
    char* userBuf = (char*)data->UserData;
    if (userBuf) { snprintf(userBuf, data->BufSize, "%s", text.c_str()); }
    return 0;
}

static void DrawLogContent(bool fill_screen) {
    if (ImGui::Button("清空日志")) { ClearJsLogs(); s_visibleLogs.clear(); }
    ImGui::SameLine();
    if (ImGui::Button("导出至本地")) {
        std::string allLogs; for (const auto& log : s_visibleLogs) { allLogs += log + "\n"; }
        WriteLogToFile(allLogs.c_str()); ImGui::OpenPopup("导出成功");
    }
    ImGui::SameLine();
    if (ImGui::Button("复制到剪贴板")) {
        std::string allLogs; for (const auto& log : s_visibleLogs) { allLogs += log + "\n"; }
        CopyLogToClipboard(allLogs.c_str()); ImGui::OpenPopup("复制成功");
    }

    if (ImGui::BeginPopupModal("导出成功", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("日志已成功保存至:\n%s", GetLogFilePath()); ImGui::Spacing();
        if (ImGui::Button("确定", ImVec2(-1, 40))) { ImGui::CloseCurrentPopup(); } ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("复制成功", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("日志已复制到系统剪贴板！"); ImGui::Spacing();
        if (ImGui::Button("确定", ImVec2(-1, 40))) { ImGui::CloseCurrentPopup(); } ImGui::EndPopup();
    }
    
    auto newLogs = DrainJsLogs();
    for (auto& log : newLogs) { s_visibleLogs.push_back(log); if (s_visibleLogs.size() > 100) s_visibleLogs.pop_front(); }
    
    float height = fill_screen ? ImGui::GetContentRegionAvail().y : (ImGui::GetContentRegionAvail().y - 5.0f);
    ImGui::BeginChild("Logs", ImVec2(-1, height), true);
    for (const auto& log : s_visibleLogs) {
        if (log.find("[error]") != std::string::npos || log.find("[ERROR]") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", log.c_str());
        } else {
            ImGui::TextWrapped("%s", log.c_str());
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

static void DrawLogArea() {
    ImGui::Spacing(); ImGui::Separator(); ImGui::TextDisabled("运行日志");
    DrawLogContent(false);
}

static void SaveTemplatesToFile() {
    std::string logPath = GetLogFilePath();
    if (logPath.empty()) return;
    size_t pos = logPath.find_last_of('/');
    if (pos == std::string::npos) return;
    std::string dir = logPath.substr(0, pos);
    std::string path = dir + "/modmenu_templates.ini";
    
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        for (auto& t : s_custom_templates) {
            std::string code = t.second;
            for (size_t i = 0; i < code.length(); ++i) {
                if (code[i] == '\n') { code.replace(i, 1, "\\n"); i++; }
                else if (code[i] == '\r') { code.erase(i, 1); i--; }
            }
            fprintf(f, "%s|%s\n", t.first.c_str(), code.c_str());
        }
        fclose(f);
    }
}

static void LoadTemplatesFromFile() {
    s_custom_templates.clear();
    std::string logPath = GetLogFilePath();
    if (logPath.empty()) return;
    size_t pos = logPath.find_last_of('/');
    if (pos == std::string::npos) return;
    std::string dir = logPath.substr(0, pos);
    std::string path = dir + "/modmenu_templates.ini";

    FILE* f = fopen(path.c_str(), "r");
    if (f) {
        char line[16384];
        while (fgets(line, sizeof(line), f)) {
            std::string str(line);
            if (!str.empty() && str.back() == '\n') str.pop_back();
            
            size_t sep = str.find('|');
            if (sep != std::string::npos) {
                std::string name = str.substr(0, sep);
                std::string code = str.substr(sep + 1);
                size_t p = 0;
                while ((p = code.find("\\n", p)) != std::string::npos) {
                    code.replace(p, 2, "\n");
                    p++;
                }
                s_custom_templates.push_back({name, code});
            }
        }
        fclose(f);
    }
}

static void PerformSilentSave() {
    char configStr[2048] = {0};
    snprintf(configStr, sizeof(configStr), 
        "ui_scale=%.2f\ntheme=%d\nshow_fps=%d\nfps_limit=%d\nfab_x=%.1f\nfab_y=%.1f\npenetration=%d\nhide_injection=%d\n"
        "tab_script=%d\ntab_template=%d\ntab_manage=%d\ntab_settings=%d\ntab_log=%d\n"
        "auto_stop=%d\nauto_stop_int=%d\n", 
        s_uiScale, s_themeIdx, s_showFps ? 1 : 0, s_fps_limit_idx, g_fab_x, g_fab_y, s_touch_penetration ? 1 : 0, g_hide_injection_features ? 1 : 0,
        s_show_tab_script ? 1 : 0, s_show_tab_template ? 1 : 0, s_show_tab_manage ? 1 : 0, s_show_tab_settings ? 1 : 0, s_show_tab_log ? 1 : 0,
        s_auto_stop_enable ? 1 : 0, s_auto_stop_interval);
    SaveConfigToFile(configStr);
    SaveScriptToFile(s_scriptBuf);
    SaveTemplatesToFile();
}

static void DrawScriptTab() {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Frida GumJS: %s", IsJsEngineReady() ? "已加载" : "未加载");
    ImGui::SameLine(); ImGui::Text("| 日志 %d 条", GetJsLogCount());
    ImGui::Separator(); ImGui::TextDisabled("自定义 JS 脚本区");
    
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##script", s_scriptBuf, sizeof(s_scriptBuf), ImVec2(-1, 150),
                              ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways,
                              InputTextCallback, s_scriptBuf);
    if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_scriptBuf, sizeof(s_scriptBuf)); KeyboardBridge::Show(); }
    if (ImGui::IsItemDeactivated()) { KeyboardBridge::Hide(); }

    ImGui::Spacing();
    if (ImGui::Button("执行脚本", ImVec2(-1, 50))) { 
        ExecuteJs(s_scriptBuf, "自定义脚本"); 
        s_auto_stop_timer = ImGui::GetTime();
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::TextDisabled("保存为模板");
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    ImGui::InputTextWithHint("##tname", "请输入模板名称...", s_new_template_name, sizeof(s_new_template_name), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_new_template_name);
    if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_new_template_name, sizeof(s_new_template_name)); KeyboardBridge::Show(); }
    if (ImGui::IsItemDeactivated()) { KeyboardBridge::Hide(); }
    
    ImGui::SameLine();
    if (ImGui::Button("保存", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        KeyboardBridge::Hide(); 
        if (strlen(s_new_template_name) == 0) {
            snprintf(s_new_template_name, sizeof(s_new_template_name), "未命名脚本_%d", (int)ImGui::GetTime());
        }
        s_custom_templates.push_back({s_new_template_name, s_scriptBuf});
        SaveTemplatesToFile();
        s_visibleLogs.push_back("[System] 模板已成功保存至内存和本地: " + std::string(s_new_template_name));
        snprintf(s_new_template_name, sizeof(s_new_template_name), ""); 
    }

    DrawLogArea();
}

static void DrawTemplateTab() {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "脚本模板库");
    ImGui::Separator();

    if (s_custom_templates.empty()) {
        ImGui::TextDisabled("暂无自定义模板。请在“脚本”页输入名字并点击保存。");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
        if (ImGui::Button("清空所有自定义脚本", ImVec2(-1, 30))) {
            s_custom_templates.clear();
            SaveTemplatesToFile();
        }
        ImGui::PopStyleColor(2);
        ImGui::Spacing();

        ImGui::BeginChild("TemplateList", ImVec2(-1, 200), true);
        for (int i = 0; i < (int)s_custom_templates.size(); ++i) {
            ImGui::PushID(s_custom_templates[i].first.c_str());
            if (ImGui::Button(s_custom_templates[i].first.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 45, 40))) {
                ExecuteJs(s_custom_templates[i].second, "模板-" + s_custom_templates[i].first);
                s_auto_stop_timer = ImGui::GetTime(); 
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
            if (ImGui::Button("×", ImVec2(35, 40))) { s_delete_template_idx = i; }
            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }
        ImGui::EndChild();

        if (s_delete_template_idx >= 0 && s_delete_template_idx < s_custom_templates.size()) {
            s_custom_templates.erase(s_custom_templates.begin() + s_delete_template_idx);
            SaveTemplatesToFile();
            s_delete_template_idx = -1; 
        }
    }

    DrawLogArea();
}

static void DrawManageTab() {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "脚本运行管理");
    ImGui::Separator();
    
    auto scripts = GetActiveScripts();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
    if (ImGui::Button("停止并清理所有脚本", ImVec2(-1, 45))) {
        StopAllScripts();
        s_auto_stop_timer = ImGui::GetTime(); 
    }
    ImGui::PopStyleColor(2);
    
    ImGui::Spacing();
    
    if (scripts.empty()) {
        ImGui::TextDisabled("当前没有正在运行的脚本。\n去“脚本”页写下你的第一段代码吧！");
    } else {
        ImGui::BeginChild("ScriptList", ImVec2(-1, 0), true);
        for (auto& pair : scripts) {
            ImGui::PushID(pair.first); 
            ImGui::AlignTextToFramePadding();
            ImGui::Text("ID: %d | %s", pair.first, pair.second.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 100); 
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
            if (ImGui::Button("停止", ImVec2(80, 30))) { StopScript(pair.first); }
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
}

static void DrawLogTab() {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "日志输出");
    ImGui::SameLine(); ImGui::TextDisabled("(共 %d 条)", GetJsLogCount());
    ImGui::Separator();
    DrawLogContent(true);
}

static void DrawSettingsTab() {
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
        
        //  强制常驻设置页
        bool true_val = true;
        ImGui::BeginDisabled();
        ImGui::Checkbox("设置页(常驻)", &true_val);
        ImGui::EndDisabled();
    }

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
        
        s_auto_stop_enable = false;
        s_auto_stop_interval = 5;

        g_fab_x = -1; 
        g_fab_y = -1;
        snprintf(s_scriptBuf, sizeof(s_scriptBuf), "console.log('Hello Frida GumJS!');\n");
        
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

void DrawAll() {
    static bool s_styleApplied = false;
    if (!s_styleApplied) {
        ImGui::GetStyle().ScrollbarSize = 50.0f;
        s_styleApplied = true;
    }

    static bool s_configLoaded = false;
    if (!s_configLoaded) {
        std::string configStr = GetConfigCache();
        if (!configStr.empty()) {
            char line[1024];
            const char* ptr = configStr.c_str();
            while (*ptr) {
                size_t lineLen = strcspn(ptr, "\n");
                if (lineLen > 0 && lineLen < sizeof(line)) {
                    snprintf(line, sizeof(line), "%.*s", (int)lineLen, ptr);
                    char* eq = strchr(line, '=');
                    if (eq) {
                        *eq = '\0';
                        const char* key = line; const char* val = eq + 1;
                        if (strcmp(key, "ui_scale") == 0) { 
                            s_uiScale = atof(val); 
                            ImGui::GetIO().FontGlobalScale = s_uiScale; 
                        }
                        else if (strcmp(key, "theme") == 0) { s_themeIdx = atoi(val); }
                        else if (strcmp(key, "show_fps") == 0) { s_showFps = (atoi(val) != 0); }
                        else if (strcmp(key, "fps_limit") == 0) { s_fps_limit_idx = atoi(val); }
                        else if (strcmp(key, "fab_x") == 0) { g_fab_x = atof(val); }
                        else if (strcmp(key, "fab_y") == 0) { g_fab_y = atof(val); }
                        else if (strcmp(key, "penetration") == 0) { 
                            s_touch_penetration = (atoi(val) != 0); 
                            g_touch_penetration = s_touch_penetration;
                        }
                        else if (strcmp(key, "hide_injection") == 0) { g_hide_injection_features = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_script") == 0) { s_show_tab_script = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_template") == 0) { s_show_tab_template = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_manage") == 0) { s_show_tab_manage = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_settings") == 0) { s_show_tab_settings = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_log") == 0) { s_show_tab_log = (atoi(val) != 0); }
                        else if (strcmp(key, "auto_stop") == 0) { s_auto_stop_enable = (atoi(val) != 0); }
                        else if (strcmp(key, "auto_stop_int") == 0) { s_auto_stop_interval = atoi(val); }
                    }
                }
                ptr += lineLen; if (*ptr == '\n') ptr++;
            }
        }
        ApplyTheme(s_themeIdx); 
        std::string scriptStr = GetScriptCache();
        if (!scriptStr.empty()) { snprintf(s_scriptBuf, sizeof(s_scriptBuf), "%s", scriptStr.c_str()); }
        
        LoadTemplatesFromFile();
        s_auto_stop_timer = ImGui::GetTime();
        s_configLoaded = true;

        // 强制常驻
        s_show_tab_settings = true;
    }

    //  自动卸载检测逻辑
    if (s_auto_stop_enable) {
        if (ImGui::GetTime() - s_auto_stop_timer >= s_auto_stop_interval) {
            StopAllScripts();
            s_auto_stop_timer = ImGui::GetTime(); 
        }
    }

    static double last_save_time = ImGui::GetTime();
    if (ImGui::GetTime() - last_save_time >= 15.0) {
        last_save_time = ImGui::GetTime();
        PerformSilentSave();
    }

    if (ImGui::BeginTabBar("MainTabs")) {
        if (s_show_tab_script && ImGui::BeginTabItem("脚本")) { DrawScriptTab();   ImGui::EndTabItem(); }
        if (s_show_tab_template && ImGui::BeginTabItem("模板")) { DrawTemplateTab(); ImGui::EndTabItem(); }
        if (s_show_tab_manage && ImGui::BeginTabItem("管理")) { DrawManageTab();   ImGui::EndTabItem(); }
        if (s_show_tab_log && ImGui::BeginTabItem("日志")) { DrawLogTab(); ImGui::EndTabItem(); }
        if (s_show_tab_settings && ImGui::BeginTabItem("设置")) { DrawSettingsTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

}