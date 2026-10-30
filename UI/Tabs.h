#pragma once
#include <vector>
#include <string>
#include "imgui.h"

namespace Tabs {
    extern bool s_show_tab_script;
    extern bool s_show_tab_template;
    extern bool s_show_tab_manage;
    extern bool s_show_tab_settings; 
    extern bool s_show_tab_log;
    extern bool s_show_tab_presets;

    extern char s_default_so_name[64];

    extern bool s_open_native_call_modal;
    extern char s_nc_offset[32];
    extern char s_nc_hook_offset[32];
    extern int s_nc_arg_count;
    extern char s_nc_arg_count_str[8];
    extern char s_nc_arg_content[8][64];
    extern int s_nc_arg_type[8]; 
    extern char s_nc_call_count_str[8];
    extern char s_nc_interval_str[8];
    extern const char* s_arg_types[4];

    extern bool s_open_dump_modal;
    extern int s_dump_mode_idx;
    extern const char* s_dump_modes[3];

    extern bool s_auto_stop_enable;
    extern int s_auto_stop_interval; 
    extern double s_auto_stop_timer;

    extern char s_scriptBuf[16384];
    extern float s_uiScale;
    extern bool  s_showFps; 
    extern int   s_fps_limit_idx;
    extern int   s_themeIdx;    
    extern bool  s_touch_penetration;

    extern std::vector<std::pair<std::string, std::string>> s_custom_templates;
    extern char  s_new_template_name[64]; 
    extern int   s_delete_template_idx; 

    void ApplyTheme(int idx);
    int InputTextCallback(ImGuiInputTextCallbackData* data);
    void PushLog(const std::string& log);

    void DrawScriptTab();
    void DrawTemplateTab();
    void DrawManageTab();
    void DrawPresetsTab();
    void DrawNativeCallModal();
    void DrawDumpModal();
    void DrawLogTab();
    void DrawLogContent(bool fill_screen);
    void DrawLogArea();
    void DrawSettingsTab();
    
    void SaveTemplatesToFile();
    void LoadTemplatesFromFile();
    void PerformSilentSave();

    void DrawAll();
}