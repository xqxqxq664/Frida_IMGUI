#include "Tabs.h"
#include "KeyboardBridge.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

extern bool IsJsEngineReady();
extern int GetJsLogCount();
extern void ExecuteJs(const std::string& code, const std::string& name = "");
extern std::vector<std::pair<int, std::string>> GetActiveScripts();
extern void StopScript(int id);
extern void StopAllScripts();

namespace Tabs {

    void DrawScriptTab() {
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
            PushLog("[System] 模板已成功保存至内存和本地: " + std::string(s_new_template_name));
            snprintf(s_new_template_name, sizeof(s_new_template_name), ""); 
        }

        DrawLogArea();
    }

    void DrawTemplateTab() {
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

    void DrawManageTab() {
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
}