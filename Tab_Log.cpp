#include "Tabs.h"
#include <deque>
#include <string>
#include <cstring>
#include <cstdio>

extern int GetJsLogCount();
extern void ClearJsLogs();
extern std::deque<std::string> DrainJsLogs();
extern void WriteLogToFile(const char* logContent);
extern void CopyLogToClipboard(const char* logContent);
extern const char* GetLogFilePath();

namespace Tabs {
    static std::deque<std::string> s_visibleLogs;

    void PushLog(const std::string& log) {
        s_visibleLogs.push_back(log);
        if (s_visibleLogs.size() > 2000) s_visibleLogs.pop_front();
    }

    void DrawLogContent(bool fill_screen) {
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
        for (auto& log : newLogs) { 
            s_visibleLogs.push_back(log); 
            if (s_visibleLogs.size() > 2000) s_visibleLogs.pop_front();
        }
        
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

    void DrawLogArea() {
        ImGui::Spacing(); ImGui::Separator(); ImGui::TextDisabled("运行日志");
        DrawLogContent(false);
    }

    void DrawLogTab() {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "日志输出");
        ImGui::SameLine(); ImGui::TextDisabled("(共 %d 条)", GetJsLogCount());
        ImGui::Separator();
        DrawLogContent(true);
    }
}