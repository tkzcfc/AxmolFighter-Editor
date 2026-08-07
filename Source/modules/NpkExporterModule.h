#pragma once

#include "modules/IEditorModule.h"
#include "tools/NpkExporterService.h"

#include <string>
#include <vector>

namespace editor
{

class NpkExporterModule final : public IEditorModule
{
public:
    static constexpr const char* PanelId = "NPK Exporter";

    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

private:
    void savePreferences(EditorContext& context);
    void addLocalLog(NpkExportLogLevel level, std::string text);
    static std::string defaultOutputRoot(EditorContext& context);

    NpkExporterService m_service;
    std::vector<NpkExportLogEntry> m_logs;
    NpkExportProgress m_progress;
    std::string m_npkDirectory;
    std::string m_outputRoot;
    std::string m_imgFilter;
    std::string m_whitelistPath;
    std::string m_failedListPath;
    bool m_overwriteExisting = false;
    bool m_initialized = false;
};

}  // namespace editor
