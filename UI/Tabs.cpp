#include "Tabs.h"
#include "UIManager.h"
#include "KeyboardBridge.h"
#include "presets.h"

#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

extern const char* JS_THREAD_CALL;
extern const char* JS_DIRECT_CALL;

extern void SaveConfigToFile(const char* content);
extern void SaveScriptToFile(const char* content);
extern const char* GetConfigCache();
extern const char* GetScriptCache();
extern const char* GetLogFilePath();
extern void StopAllScripts();

extern float g_fab_x;
extern float g_fab_y;
extern bool g_touch_penetration;
extern bool g_hide_injection_features;

namespace Tabs {

bool s_show_tab_script = true;
bool s_show_tab_template = true;
bool s_show_tab_manage = true;
bool s_show_tab_settings = true; 
bool s_show_tab_log = true;
bool s_show_tab_presets = true;

char s_default_so_name[64] = "libil2cpp.so";

bool s_open_native_call_modal = false;
char s_nc_offset[32] = "0x0";
char s_nc_hook_offset[32] = "";
int s_nc_arg_count = 1;
char s_nc_arg_count_str[8] = "1";
char s_nc_arg_content[8][64] = {"0x0"};
int s_nc_arg_type[8] = {2, 0, 0, 0, 0, 0, 0, 0}; 
char s_nc_call_count_str[8] = "1";
char s_nc_interval_str[8] = "10";
const char* s_arg_types[] = { "int", "float", "pointer", "string" };

bool s_open_dump_modal = false;
int s_dump_mode_idx = 1;
const char* s_dump_modes[] = { "全面版 (所有Dll)", "轻量版 (仅Assembly-CSharp)", "超轻量版 (无偏移量)" };

bool s_auto_stop_enable = false;
int s_auto_stop_interval = 5; 
double s_auto_stop_timer = 0.0;

char s_scriptBuf[16384] = R"raw(console.log("Frida版本: " + Frida.version);
console.log("目标架构: " + Process.arch);

if (Java.available) {
    Java.perform(function() {
        var app = Java.use('android.app.ActivityThread').currentApplication();
        var pkg = app ? app.getPackageName() : "未获取到";
        console.log("目标包名: " + pkg);
    });
})raw";

float s_uiScale = 1.0f;
bool  s_showFps = false; 
int   s_fps_limit_idx = 0;
int   s_themeIdx = 2;    
bool  s_touch_penetration = false;

std::vector<std::pair<std::string, std::string>> s_custom_templates;
char  s_new_template_name[64] = ""; 
int   s_delete_template_idx = -1; 

int InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (!KeyboardBridge::HasPending()) return 0;
    std::string text = KeyboardBridge::TakePending();
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text.c_str());
    char* userBuf = (char*)data->UserData;
    if (userBuf) { snprintf(userBuf, data->BufSize, "%s", text.c_str()); }
    return 0;
}

void SaveTemplatesToFile() {
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

void LoadTemplatesFromFile() {
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

void PerformSilentSave() {
    char configStr[8192] = {0};
    int len = snprintf(configStr, sizeof(configStr), 
        "ui_scale=%.2f\ntheme=%d\nshow_fps=%d\nfps_limit=%d\nfab_x=%.1f\nfab_y=%.1f\npenetration=%d\nhide_injection=%d\n"
        "tab_script=%d\ntab_template=%d\ntab_manage=%d\ntab_settings=%d\ntab_log=%d\ntab_presets=%d\n"
        "auto_stop=%d\nauto_stop_int=%d\nso_name=%s\ndump_mode=%d\n", 
        s_uiScale, s_themeIdx, s_showFps ? 1 : 0, s_fps_limit_idx, g_fab_x, g_fab_y, s_touch_penetration ? 1 : 0, g_hide_injection_features ? 1 : 0,
        s_show_tab_script ? 1 : 0, s_show_tab_template ? 1 : 0, s_show_tab_manage ? 1 : 0, s_show_tab_settings ? 1 : 0, s_show_tab_log ? 1 : 0, s_show_tab_presets ? 1 : 0,
        s_auto_stop_enable ? 1 : 0, s_auto_stop_interval, s_default_so_name, s_dump_mode_idx);

    len += snprintf(configStr + len, sizeof(configStr) - len, 
        "nc_offset=%s\nnc_hook_offset=%s\nnc_arg_count=%d\nnc_call_count=%s\nnc_interval=%s\n",
        s_nc_offset, s_nc_hook_offset, s_nc_arg_count, s_nc_call_count_str, s_nc_interval_str);

    for (int i = 0; i < 8; ++i) {
        len += snprintf(configStr + len, sizeof(configStr) - len, "nc_arg_type_%d=%d\nnc_arg_content_%d=%s\n", i, s_nc_arg_type[i], i, s_nc_arg_content[i]);
    }

    SaveConfigToFile(configStr);
    SaveScriptToFile(s_scriptBuf);
    SaveTemplatesToFile();
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
                        if (strcmp(key, "ui_scale") == 0) { s_uiScale = atof(val); ImGui::GetIO().FontGlobalScale = s_uiScale; }
                        else if (strcmp(key, "theme") == 0) { s_themeIdx = atoi(val); }
                        else if (strcmp(key, "show_fps") == 0) { s_showFps = (atoi(val) != 0); }
                        else if (strcmp(key, "fps_limit") == 0) { s_fps_limit_idx = atoi(val); }
                        else if (strcmp(key, "fab_x") == 0) { g_fab_x = atof(val); }
                        else if (strcmp(key, "fab_y") == 0) { g_fab_y = atof(val); }
                        else if (strcmp(key, "penetration") == 0) { s_touch_penetration = (atoi(val) != 0); g_touch_penetration = s_touch_penetration; }
                        else if (strcmp(key, "hide_injection") == 0) { g_hide_injection_features = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_script") == 0) { s_show_tab_script = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_template") == 0) { s_show_tab_template = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_manage") == 0) { s_show_tab_manage = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_settings") == 0) { s_show_tab_settings = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_log") == 0) { s_show_tab_log = (atoi(val) != 0); }
                        else if (strcmp(key, "tab_presets") == 0) { s_show_tab_presets = (atoi(val) != 0); }
                        else if (strcmp(key, "auto_stop") == 0) { s_auto_stop_enable = (atoi(val) != 0); }
                        else if (strcmp(key, "auto_stop_int") == 0) { s_auto_stop_interval = atoi(val); }
                        else if (strcmp(key, "so_name") == 0) { snprintf(s_default_so_name, sizeof(s_default_so_name), "%s", val); }
                        else if (strcmp(key, "dump_mode") == 0) { s_dump_mode_idx = atoi(val); }
                        else if (strcmp(key, "nc_offset") == 0) { snprintf(s_nc_offset, sizeof(s_nc_offset), "%s", val); }
                        else if (strcmp(key, "nc_hook_offset") == 0) { snprintf(s_nc_hook_offset, sizeof(s_nc_hook_offset), "%s", val); }
                        else if (strcmp(key, "nc_arg_count") == 0) { 
                            s_nc_arg_count = atoi(val); 
                            if(s_nc_arg_count < 1) s_nc_arg_count = 1;
                            if(s_nc_arg_count > 8) s_nc_arg_count = 8;
                            snprintf(s_nc_arg_count_str, sizeof(s_nc_arg_count_str), "%d", s_nc_arg_count); 
                        }
                        else if (strcmp(key, "nc_call_count") == 0) { snprintf(s_nc_call_count_str, sizeof(s_nc_call_count_str), "%s", val); }
                        else if (strcmp(key, "nc_interval") == 0) { snprintf(s_nc_interval_str, sizeof(s_nc_interval_str), "%s", val); }
                        else if (strncmp(key, "nc_arg_type_", 12) == 0) {
                            int idx = atoi(key + 12);
                            if (idx >= 0 && idx < 8) s_nc_arg_type[idx] = atoi(val);
                        }
                        else if (strncmp(key, "nc_arg_content_", 15) == 0) {
                            int idx = atoi(key + 15);
                            if (idx >= 0 && idx < 8) snprintf(s_nc_arg_content[idx], sizeof(s_nc_arg_content[idx]), "%s", val);
                        }
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
        s_show_tab_settings = true;
    }

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
        if (s_show_tab_presets && ImGui::BeginTabItem("预设")) { DrawPresetsTab(); ImGui::EndTabItem(); }
        if (s_show_tab_log && ImGui::BeginTabItem("日志")) { DrawLogTab(); ImGui::EndTabItem(); }
        if (s_show_tab_settings && ImGui::BeginTabItem("设置")) { DrawSettingsTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

}