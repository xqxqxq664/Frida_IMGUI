#include "Tabs.h"
#include "UIManager.h"
#include "KeyboardBridge.h"
#include "presets.h"
#include <cstring>
#include <string>
#include <cstdlib>
#include <algorithm>

extern const char* JS_THREAD_CALL;
extern const char* JS_DIRECT_CALL;
extern const char* JS_FULL_DUMP;
extern const char* JS_LITE_DUMP;
extern const char* JS_ULTRA_LITE_DUMP;
extern void ExecuteJs(const std::string& code, const std::string& name = "");

namespace Tabs {

    void DrawNativeCallModal() {
        if (s_open_native_call_modal) {
            ImGui::OpenPopup("主动调用Native函数");
            s_open_native_call_modal = false;
        }

        ImGui::SetNextWindowSize(ImVec2(750, 550), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("主动调用Native函数", NULL, 0)) {
            ImGui::Text("当前 So: %s", s_default_so_name);
            ImGui::Separator();

            ImGui::Text("偏移量");
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##offset", s_nc_offset, sizeof(s_nc_offset), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_nc_offset);
            if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_nc_offset, sizeof(s_nc_offset)); KeyboardBridge::Show(); }
            if (ImGui::IsItemDeactivated()) { KeyboardBridge::Hide(); }

            ImGui::Text("hook刷新偏移量(留空使用普通方案)");
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##hook_offset", s_nc_hook_offset, sizeof(s_nc_hook_offset), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_nc_hook_offset);
            if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_nc_hook_offset, sizeof(s_nc_hook_offset)); KeyboardBridge::Show(); }
            if (ImGui::IsItemDeactivated()) { KeyboardBridge::Hide(); }

            ImGui::Spacing();
            ImGui::Text("参数数量");
            ImGui::SameLine();
            if (ImGui::Button("-##argcount_minus")) { s_nc_arg_count = std::max(1, s_nc_arg_count - 1); snprintf(s_nc_arg_count_str, sizeof(s_nc_arg_count_str), "%d", s_nc_arg_count); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::InputText("##arg_count", s_nc_arg_count_str, sizeof(s_nc_arg_count_str), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_nc_arg_count_str);
            if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_nc_arg_count_str, sizeof(s_nc_arg_count_str)); KeyboardBridge::Show(); }
            if (ImGui::IsItemDeactivated()) {
                KeyboardBridge::Hide();
                s_nc_arg_count = atoi(s_nc_arg_count_str);
                if (s_nc_arg_count < 1) s_nc_arg_count = 1;
                if (s_nc_arg_count > 8) s_nc_arg_count = 8;
                snprintf(s_nc_arg_count_str, sizeof(s_nc_arg_count_str), "%d", s_nc_arg_count);
            }
            ImGui::SameLine();
            if (ImGui::Button("+##argcount_plus")) { s_nc_arg_count = std::min(8, s_nc_arg_count + 1); snprintf(s_nc_arg_count_str, sizeof(s_nc_arg_count_str), "%d", s_nc_arg_count); }

            for (int i = 0; i < s_nc_arg_count; ++i) {
                ImGui::PushID(i);
                ImGui::Separator();
                ImGui::Text("参数 %d", i + 1);
                
                ImGui::SetNextItemWidth(200);
                ImGui::InputText("内容", s_nc_arg_content[i], sizeof(s_nc_arg_content[i]), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_nc_arg_content[i]);
                if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_nc_arg_content[i], sizeof(s_nc_arg_content[i])); KeyboardBridge::Show(); }
                if (ImGui::IsItemDeactivated()) { KeyboardBridge::Hide(); }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::Combo("类型", &s_nc_arg_type[i], s_arg_types, 4);
                ImGui::PopID();
            }

            ImGui::Separator();
            ImGui::Text("调用次数");
            ImGui::SameLine();
            if (ImGui::Button("-##call_minus")) { int c = atoi(s_nc_call_count_str); c = std::max(1, c - 1); snprintf(s_nc_call_count_str, sizeof(s_nc_call_count_str), "%d", c); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::InputText("##call_count", s_nc_call_count_str, sizeof(s_nc_call_count_str), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_nc_call_count_str);
            if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_nc_call_count_str, sizeof(s_nc_call_count_str)); KeyboardBridge::Show(); }
            if (ImGui::IsItemDeactivated()) {
                KeyboardBridge::Hide();
                int c = atoi(s_nc_call_count_str);
                if (c < 1) c = 1;
                snprintf(s_nc_call_count_str, sizeof(s_nc_call_count_str), "%d", c);
            }
            ImGui::SameLine();
            if (ImGui::Button("+##call_plus")) { int c = atoi(s_nc_call_count_str); c++; snprintf(s_nc_call_count_str, sizeof(s_nc_call_count_str), "%d", c); }

            ImGui::Text("间隔(毫秒)");
            ImGui::SameLine();
            if (ImGui::Button("-##interval_minus")) { int v = atoi(s_nc_interval_str); v = std::max(0, v - 10); snprintf(s_nc_interval_str, sizeof(s_nc_interval_str), "%d", v); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::InputText("##interval", s_nc_interval_str, sizeof(s_nc_interval_str), ImGuiInputTextFlags_CallbackAlways, InputTextCallback, s_nc_interval_str);
            if (ImGui::IsItemActivated()) { KeyboardBridge::SetActiveBuffer(s_nc_interval_str, sizeof(s_nc_interval_str)); KeyboardBridge::Show(); }
            if (ImGui::IsItemDeactivated()) {
                KeyboardBridge::Hide();
                int v = atoi(s_nc_interval_str);
                if (v < 0) v = 0;
                snprintf(s_nc_interval_str, sizeof(s_nc_interval_str), "%d", v);
            }
            ImGui::SameLine();
            if (ImGui::Button("+##interval_plus")) { int v = atoi(s_nc_interval_str); v += 10; snprintf(s_nc_interval_str, sizeof(s_nc_interval_str), "%d", v); }

            ImGui::Spacing();
            if (ImGui::Button("生成并执行", ImVec2(150, 40))) {
                std::string typesStr = "[";
                std::string argsStr = "[";
                for (int i = 0; i < s_nc_arg_count; ++i) {
                    if (s_nc_arg_type[i] == 0) typesStr += "'int'";
                    else if (s_nc_arg_type[i] == 1) typesStr += "'float'";
                    else if (s_nc_arg_type[i] == 2) typesStr += "'pointer'";
                    else if (s_nc_arg_type[i] == 3) typesStr += "'pointer'";
                    if (i < s_nc_arg_count - 1) typesStr += ",";

                    if (s_nc_arg_type[i] == 0 || s_nc_arg_type[i] == 1) argsStr += std::string(s_nc_arg_content[i]);
                    else if (s_nc_arg_type[i] == 2) argsStr += "ptr(\"" + std::string(s_nc_arg_content[i]) + "\")";
                    else if (s_nc_arg_type[i] == 3) argsStr += "Memory.allocUtf8String(\"" + std::string(s_nc_arg_content[i]) + "\")";
                    if (i < s_nc_arg_count - 1) argsStr += ",";
                }
                typesStr += "]";
                argsStr += "]";

                bool has_hook = strlen(s_nc_hook_offset) > 0 && strcmp(s_nc_hook_offset, "0x") != 0 && strcmp(s_nc_hook_offset, "0X") != 0;
                std::string code = has_hook ? JS_THREAD_CALL : JS_DIRECT_CALL;

                auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
                    size_t start_pos = 0;
                    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
                        str.replace(start_pos, from.length(), to);
                        start_pos += to.length();
                    }
                };

                replaceAll(code, "{{SO_NAME}}", s_default_so_name);
                replaceAll(code, "{{TARGET_OFFSET}}", s_nc_offset);
                replaceAll(code, "{{HOOK_OFFSET}}", s_nc_hook_offset);
                replaceAll(code, "{{ARG_TYPES}}", typesStr);
                replaceAll(code, "{{ARG_VALUES}}", argsStr);
                replaceAll(code, "{{CALL_COUNT}}", s_nc_call_count_str);
                replaceAll(code, "{{INTERVAL_MS}}", s_nc_interval_str);

                ExecuteJs(code, "NativeCall");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("取消", ImVec2(120, 40))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawDumpModal() {
        if (s_open_dump_modal) {
            ImGui::OpenPopup("il2cpp api dump");
            s_open_dump_modal = false;
        }

        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("il2cpp api dump", NULL, 0)) {
            ImGui::Text("Dump 模式选择");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##dump_mode", &s_dump_mode_idx, s_dump_modes, 3);
            
            ImGui::Spacing(); ImGui::Spacing();
            
            if (ImGui::Button("开始 Dump", ImVec2(150, 40))) {
                std::string code;
                if (s_dump_mode_idx == 0) code = JS_FULL_DUMP;
                else if (s_dump_mode_idx == 1) code = JS_LITE_DUMP;
                else code = JS_ULTRA_LITE_DUMP;

                ExecuteJs(code, "Il2cppDump");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("取消", ImVec2(120, 40))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawPresetsTab() {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "预设脚本库");
        ImGui::Separator();
        
        if (ImGui::Button("主动调用Native函数", ImVec2(-1, 50))) {
            s_open_native_call_modal = true;
        }
        if (ImGui::Button("il2cpp api dump", ImVec2(-1, 50))) {
            s_open_dump_modal = true;
        }
        
        ImGui::Spacing();
        DrawNativeCallModal();
        DrawDumpModal();
        DrawLogArea();
    }
}