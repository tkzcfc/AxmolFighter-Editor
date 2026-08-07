#include "modules/NpkExporterModule.h"

#include "core/EditorContext.h"
#include "core/EditorPreferencesService.h"
#include "modules/PanelVisibilityService.h"
#include "tools/DirectoryPicker.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <filesystem>
#include <iterator>

namespace editor
{
namespace
{

ImVec4 logColor(NpkExportLogLevel level)
{
    switch (level)
    {
    case NpkExportLogLevel::Warning:
        return ImVec4(1.0f, 0.78f, 0.28f, 1.0f);
    case NpkExportLogLevel::Error:
        return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    default:
        return ImVec4(0.55f, 0.90f, 0.55f, 1.0f);
    }
}

const char* logPrefix(NpkExportLogLevel level)
{
    switch (level)
    {
    case NpkExportLogLevel::Warning:
        return "WARN";
    case NpkExportLogLevel::Error:
        return "ERROR";
    default:
        return "INFO";
    }
}

}  // namespace

void NpkExporterModule::onAttach(EditorContext& context)
{
    if (m_initialized)
        return;

    if (auto preferences = context.services().get<EditorPreferencesService>())
    {
        m_npkDirectory = preferences->npkExporterDirectory();
        m_outputRoot = preferences->npkExporterOutputRoot();
        m_imgFilter = preferences->npkExporterFilter();
    }
    if (m_outputRoot.empty())
        m_outputRoot = defaultOutputRoot(context);
    if (m_whitelistPath.empty())
    {
        const std::filesystem::path candidate =
            context.settings().resolvedResourceRoot() / "npk_export_list.json";
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error))
            m_whitelistPath = candidate.lexically_normal().generic_string();
    }
    if (m_failedListPath.empty())
    {
        const std::filesystem::path candidate =
            context.settings().resolvedResourceRoot() / "npk_export_failed.json";
        m_failedListPath = candidate.lexically_normal().generic_string();
    }
    m_initialized = true;
}

void NpkExporterModule::onDetach(EditorContext& context)
{
    savePreferences(context);
    m_service.shutdown();
    m_initialized = false;
}

void NpkExporterModule::onUpdate(EditorContext& context, float)
{
    if (!m_initialized)
        onAttach(context);

    std::vector<NpkExportLogEntry> newLogs;
    m_service.drainLogs(newLogs);
    m_logs.insert(m_logs.end(), std::make_move_iterator(newLogs.begin()), std::make_move_iterator(newLogs.end()));
    m_progress = m_service.progress();
}

void NpkExporterModule::savePreferences(EditorContext& context)
{
    if (auto preferences = context.services().get<EditorPreferencesService>())
    {
        preferences->setNpkExporterDirectory(m_npkDirectory);
        preferences->setNpkExporterOutputRoot(m_outputRoot);
        preferences->setNpkExporterFilter(m_imgFilter);
    }
}

void NpkExporterModule::addLocalLog(NpkExportLogLevel level, std::string text)
{
    m_logs.push_back({level, std::move(text)});
}

std::string NpkExporterModule::defaultOutputRoot(EditorContext& context)
{
    const std::filesystem::path resourceRoot = context.settings().resolvedResourceRoot();
    if (resourceRoot.empty())
        return {};

    std::error_code error;
    for (std::filesystem::path candidate = resourceRoot; !candidate.empty(); candidate = candidate.parent_path())
    {
        error.clear();
        if (std::filesystem::is_directory(candidate / "AxmolFighter-Client", error))
            return (candidate / "AxmolFighter-Client" / "Content" / "mugen").lexically_normal().generic_string();
        if (candidate == candidate.root_path())
            break;
    }

    // Fallback for a not-yet-created client directory.
    const std::filesystem::path repositoryRoot =
        resourceRoot.filename() == "Content" ? resourceRoot.parent_path().parent_path().parent_path()
                                               : resourceRoot.parent_path().parent_path();
    return (repositoryRoot / "AxmolFighter-Client" / "Content" / "mugen").lexically_normal().generic_string();
}

void NpkExporterModule::onImGuiRender(EditorContext& context)
{
    auto visibility = context.services().get<PanelVisibilityService>();
    if (visibility && !visibility->isOpen(PanelId))
        return;

    ImGui::SetNextWindowSize(ImVec2(760.0f, 560.0f), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin(PanelId, &open))
    {
        ImGui::End();
        if (!open && visibility)
            visibility->close(PanelId);
        return;
    }

    const bool running = m_progress.running;
    ImGui::BeginDisabled(running);

    ImGui::TextUnformatted("NPK directory");
    ImGui::PushID("NpkDirectory");
    ImGui::InputText("##Path", &m_npkDirectory);
    ImGui::SameLine();
    if (ImGui::Button("Browse...") )
    {
        std::filesystem::path selected;
        if (pickDirectory(selected))
            m_npkDirectory = selected.generic_string();
    }
    ImGui::PopID();

    ImGui::TextUnformatted("Output root");
    ImGui::PushID("OutputRoot");
    ImGui::InputText("##Path", &m_outputRoot);
    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        std::filesystem::path selected;
        if (pickDirectory(selected))
            m_outputRoot = selected.generic_string();
    }
    ImGui::PopID();

    ImGui::InputText("IMG path filter (case-sensitive)", &m_imgFilter);

    ImGui::TextUnformatted("Resource list file (optional whitelist)");
    ImGui::PushID("WhitelistPath");
    ImGui::InputText("##Whitelist", &m_whitelistPath);
    ImGui::SameLine();
    if (ImGui::Button("Detect"))
    {
        const std::filesystem::path candidate =
            context.settings().resolvedResourceRoot() / "npk_export_list.json";
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error))
            m_whitelistPath = candidate.lexically_normal().generic_string();
        else
            addLocalLog(NpkExportLogLevel::Warning,
                        "Whitelist not found: " + candidate.lexically_normal().generic_string());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        m_whitelistPath.clear();
    ImGui::PopID();
    ImGui::TextDisabled("Empty = export all IMGs. Non-empty = exact match on NPK index names.");

    ImGui::TextUnformatted("Failed IMG list output (JSON)");
    ImGui::PushID("FailedListPath");
    ImGui::InputText("##FailedList", &m_failedListPath);
    ImGui::SameLine();
    if (ImGui::Button("Default##Failed"))
    {
        const std::filesystem::path candidate =
            context.settings().resolvedResourceRoot() / "npk_export_failed.json";
        m_failedListPath = candidate.lexically_normal().generic_string();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##Failed"))
        m_failedListPath.clear();
    ImGui::SameLine();
    if (ImGui::Button("Scan All NPKs##Failed"))
    {
        std::error_code error;
        if (m_npkDirectory.empty() ||
            !std::filesystem::is_directory(std::filesystem::path(m_npkDirectory), error))
        {
            addLocalLog(NpkExportLogLevel::Error, "NPK directory does not exist.");
        }
        else if (m_failedListPath.empty())
        {
            addLocalLog(NpkExportLogLevel::Error, "Failed-list path is empty.");
        }
        else
        {
            savePreferences(context);
            m_logs.clear();
            m_progress = {};
            NpkExportRequest request;
            request.npkDirectory = std::filesystem::path(m_npkDirectory);
            request.failedListPath = std::filesystem::path(m_failedListPath);
            request.scanFailedOnly = true;
            request.mergeFailedList = false;
            if (!m_service.start(std::move(request)))
                addLocalLog(NpkExportLogLevel::Error, "Failed to start failed-IMG scan.");
        }
    }
    ImGui::PopID();
    ImGui::TextDisabled(
        "Empty = do not write on export. Scan All NPKs = validate every IMG and overwrite this JSON.");
    ImGui::Checkbox("Overwrite existing PNG/.vf files", &m_overwriteExisting);
    ImGui::TextDisabled("Unchecked (default) = skip outputs that already exist (PNG and .vf checked separately).");
    ImGui::EndDisabled();

    if (!running)
    {
        if (ImGui::Button("Export"))
        {
            std::error_code error;
            if (m_npkDirectory.empty() ||
                !std::filesystem::is_directory(std::filesystem::path(m_npkDirectory), error))
            {
                addLocalLog(NpkExportLogLevel::Error, "NPK directory does not exist.");
            }
            else if (m_outputRoot.empty())
            {
                addLocalLog(NpkExportLogLevel::Error, "Output root is empty.");
            }
            else if (!m_whitelistPath.empty() &&
                     !std::filesystem::is_regular_file(std::filesystem::path(m_whitelistPath), error))
            {
                addLocalLog(NpkExportLogLevel::Error, "Whitelist file does not exist.");
            }
            else
            {
                savePreferences(context);
                m_logs.clear();
                m_progress = {};
                NpkExportRequest request;
                request.npkDirectory = std::filesystem::path(m_npkDirectory);
                request.outputRoot = std::filesystem::path(m_outputRoot);
                request.imgFilter = m_imgFilter;
                request.overwriteExisting = m_overwriteExisting;
                if (!m_whitelistPath.empty())
                    request.whitelistPath = std::filesystem::path(m_whitelistPath);
                if (!m_failedListPath.empty())
                    request.failedListPath = std::filesystem::path(m_failedListPath);
                if (!m_service.start(std::move(request)))
                    addLocalLog(NpkExportLogLevel::Error, "Failed to start NPK export.");
            }
        }
    }
    else if (ImGui::Button("Cancel"))
    {
        m_service.cancel();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_progress.phase.empty() ? "Idle" : m_progress.phase.c_str());

    const float fraction = m_progress.totalFrames == 0
                               ? (running ? -1.0f : 0.0f)
                               : static_cast<float>(m_progress.completedFrames) /
                                     static_cast<float>(m_progress.totalFrames);
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f));
    ImGui::TextDisabled("Frames: %zu / %zu", m_progress.completedFrames, m_progress.totalFrames);

    ImGui::Separator();
    if (ImGui::BeginChild("NpkExporterLog", ImVec2(0.0f, 0.0f), true,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (const auto& entry : m_logs)
        {
            const ImVec4 color = logColor(entry.level);
            ImGui::TextColored(color, "[%s] %s", logPrefix(entry.level), entry.text.c_str());
        }
        if (running && !m_logs.empty())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
    if (!open && visibility)
    {
        if (running)
            m_service.cancel();
        visibility->close(PanelId);
    }
}

}  // namespace editor
