#include "modules/LogModule.h"

#include "core/EditorContext.h"
#include "core/EditorLogService.h"
#include "imgui.h"
#include "modules/PanelVisibilityService.h"

namespace editor
{
namespace
{
const char* levelLabel(ax::LogLevel level)
{
    switch (level)
    {
    case ax::LogLevel::Verbose:
        return "V";
    case ax::LogLevel::Debug:
        return "D";
    case ax::LogLevel::Info:
        return "I";
    case ax::LogLevel::Warn:
        return "W";
    case ax::LogLevel::Error:
        return "E";
    default:
        return "?";
    }
}

ImVec4 levelColor(ax::LogLevel level)
{
    switch (level)
    {
    case ax::LogLevel::Verbose:
        return ImVec4(0.72f, 0.72f, 0.72f, 1.0f);
    case ax::LogLevel::Debug:
        return ImVec4(0.42f, 0.82f, 1.0f, 1.0f);
    case ax::LogLevel::Info:
        return ImVec4(0.45f, 0.88f, 0.45f, 1.0f);
    case ax::LogLevel::Warn:
        return ImVec4(1.0f, 0.78f, 0.28f, 1.0f);
    case ax::LogLevel::Error:
        return ImVec4(1.0f, 0.38f, 0.38f, 1.0f);
    default:
        return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    }
}
}

void LogModule::onAttach(EditorContext& context)
{
    if (auto visibility = context.services().get<PanelVisibilityService>())
        visibility->open(PanelId);

    if (auto logService = context.services().get<EditorLogService>())
        ax::setLogOutput(logService.get());
}

void LogModule::onDetach(EditorContext&)
{
    ax::setLogOutput(nullptr);
}

void LogModule::onUpdate(EditorContext&, float) {}

void LogModule::onImGuiRender(EditorContext& context)
{
    auto visibility = context.services().get<PanelVisibilityService>();
    if (visibility && !visibility->isOpen(PanelId))
        return;

    bool open = true;
    if (!ImGui::Begin(PanelId, &open))
    {
        ImGui::End();
        if (!open && visibility)
            visibility->close(PanelId);
        return;
    }

    auto logService = context.services().get<EditorLogService>();
    if (!logService)
    {
        ImGui::TextDisabled("Log service is not available.");
        ImGui::End();
        if (!open && visibility)
            visibility->close(PanelId);
        return;
    }

    bool collapse = logService->collapseEnabled();
    if (ImGui::Checkbox("Collapse", &collapse))
        logService->setCollapseEnabled(collapse);

    ImGui::SameLine();
    bool autoScroll = logService->autoScrollEnabled();
    if (ImGui::Checkbox("Auto-scroll", &autoScroll))
        logService->setAutoScrollEnabled(autoScroll);

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        logService->clear();

    const std::size_t totalEntries = logService->entryCount();
    const auto entries = logService->snapshot(collapse);
    ImGui::SameLine();
    ImGui::TextDisabled("Showing %zu / %zu", entries.size(), totalEntries);

    ImGui::Separator();

    const std::uint64_t revision = logService->revision();
    const bool shouldAutoScroll = autoScroll && revision != m_lastRevision;
    m_lastRevision = revision;

    if (ImGui::BeginChild("LogEntriesRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                           ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("LogEntries", 3, tableFlags))
        {
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (const EditorLogService::DisplayEntry& entry : entries)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(levelColor(entry.level), "%s", levelLabel(entry.level));

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.text.c_str());

                ImGui::TableSetColumnIndex(2);
                if (collapse && entry.count > 1)
                    ImGui::Text("%zu", entry.count);
            }

            ImGui::EndTable();
        }

        if (shouldAutoScroll && !entries.empty())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
    if (!open && visibility)
        visibility->close(PanelId);
}
}  // namespace editor
