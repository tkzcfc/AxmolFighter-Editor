#include "modules/MainMenuModule.h"

#include "axmol.h"
#include "core/EditorContext.h"
#include "core/EditorPreferencesService.h"
#include "core/LayoutService.h"
#include "document_editors/DocumentEditorRegistry.h"
#include "documents/DocumentCloseService.h"
#include "documents/IEditorDocument.h"
#include "modules/LogModule.h"
#include "modules/NpkExporterModule.h"
#include "modules/PanelVisibilityService.h"
#include "imgui.h"

#include <filesystem>

namespace editor
{
namespace
{
constexpr const char* kEditorSettingsPanelId = "Editor Settings";
}

void MainMenuModule::onAttach(EditorContext&) {}

void MainMenuModule::onDetach(EditorContext&) {}

void MainMenuModule::onUpdate(EditorContext& context, float)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        saveAllDocuments(context);
    }
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
    {
        if (io.KeyShift)
            redoActiveDocument(context);
        else
            undoActiveDocument(context);
    }
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
    {
        redoActiveDocument(context);
    }
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        saveActiveDocument(context);
    }
}

void MainMenuModule::onImGuiRender(EditorContext& context)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save", "Ctrl+S"))
            saveActiveDocument(context);
        if (ImGui::MenuItem("Save All", "Ctrl+Shift+S"))
            saveAllDocuments(context);
        if (ImGui::MenuItem("Reload"))
            reloadActiveDocument(context);
        if (ImGui::MenuItem("Close"))
            closeActiveDocument(context);
        if (ImGui::MenuItem("Close All"))
            closeAllCleanDocuments(context);
        ImGui::Separator();
        if (ImGui::MenuItem("Editor Settings"))
        {
            if (auto visibility = context.services().get<PanelVisibilityService>())
                visibility->open(kEditorSettingsPanelId);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            ax::Director::getInstance()->end();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        IEditorDocument* document = context.documents().activeDocument();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, document && document->canUndo()))
            undoActiveDocument(context);
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, document && document->canRedo()))
            redoActiveDocument(context);
        if (auto preferences = context.services().get<EditorPreferencesService>())
        {
            bool showRootBounds = preferences->showRootBounds();
            if (ImGui::MenuItem("Show Root Bounds", nullptr, showRootBounds))
                preferences->setShowRootBounds(!showRootBounds);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        auto visibility = context.services().get<PanelVisibilityService>();
        if (ImGui::MenuItem("Reset Layout"))
        {
            if (auto layoutService = context.services().get<LayoutService>())
                layoutService->requestResetLayout();
        }
        ImGui::Separator();
        ImGui::MenuItem("Content", nullptr, true, false);
        ImGui::MenuItem("Scene Hierarchy", nullptr, true, false);
        ImGui::MenuItem("Inspector", nullptr, true, false);
        ImGui::MenuItem("Assets", nullptr, true, false);
        ImGui::MenuItem("Shape List", nullptr, true, false);
        const bool logOpen = visibility && visibility->isOpen(LogModule::PanelId);
        if (ImGui::MenuItem(LogModule::PanelId, nullptr, logOpen))
        {
            if (visibility)
                visibility->setOpen(LogModule::PanelId, !logOpen);
        }
        if (const auto registry = context.services().get<DocumentEditorRegistry>())
        {
            for (const DocumentBottomPanelDescriptor& panel : registry->bottomPanels())
            {
                const bool panelOpen = visibility && visibility->isOpen(panel.panelId);
                if (ImGui::MenuItem(panel.title.c_str(), nullptr, panelOpen) && visibility)
                    visibility->setOpen(panel.panelId, !panelOpen);
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools"))
    {
        auto visibility = context.services().get<PanelVisibilityService>();
        const bool exporterOpen = visibility && visibility->isOpen(NpkExporterModule::PanelId);
        if (ImGui::MenuItem(NpkExporterModule::PanelId, nullptr, exporterOpen))
        {
            if (visibility)
                visibility->setOpen(NpkExporterModule::PanelId, !exporterOpen);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        ImGui::TextDisabled("Axmol Editor");
        ImGui::EndMenu();
    }

    if (!m_statusMessage.empty())
    {
        ImGui::Separator();
        ImGui::TextUnformatted(m_statusMessage.c_str());
    }

    ImGui::EndMainMenuBar();
}

void MainMenuModule::saveActiveDocument(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    if (!document)
    {
        m_statusMessage = "No active document.";
        return;
    }

    if (document->save())
        m_statusMessage = "Saved " + document->getDisplayName();
    else
        m_statusMessage = "Save failed: " + document->lastError();
}

void MainMenuModule::saveAllDocuments(EditorContext& context)
{
    std::string message;
    std::size_t savedCount = 0;
    if (context.documents().saveAll(message, savedCount))
        m_statusMessage =
            savedCount == 0 ? "No dirty documents." : "Saved " + std::to_string(savedCount) + " document(s).";
    else
        m_statusMessage = message;
}

void MainMenuModule::undoActiveDocument(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    if (!document)
    {
        m_statusMessage = "No active document.";
        return;
    }

    if (document->undo())
        m_statusMessage = "Undid " + document->getDisplayName();
    else
        m_statusMessage = "Nothing to undo.";
}

void MainMenuModule::redoActiveDocument(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    if (!document)
    {
        m_statusMessage = "No active document.";
        return;
    }

    if (document->redo())
        m_statusMessage = "Redid " + document->getDisplayName();
    else
        m_statusMessage = "Nothing to redo.";
}

void MainMenuModule::reloadActiveDocument(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    if (!document)
    {
        m_statusMessage = "No active document.";
        return;
    }
    if (document->path().empty())
    {
        m_statusMessage = "Document path is empty.";
        return;
    }

    const std::filesystem::path path = document->path();
    if (document->open(path))
        m_statusMessage = "Reloaded " + document->getDisplayName();
    else
        m_statusMessage = "Reload failed: " + document->lastError();
}

void MainMenuModule::closeActiveDocument(EditorContext& context)
{
    if (!context.documents().activeDocument())
    {
        m_statusMessage = "No active document.";
        return;
    }

    if (auto closeService = context.services().get<DocumentCloseService>())
        closeService->requestClose(context.documents().activeIndex());
    else
        m_statusMessage = "Document close service is not available.";
}

void MainMenuModule::closeAllCleanDocuments(EditorContext& context)
{
    std::size_t dirtyCount        = 0;
    const std::size_t closedCount = context.documents().closeCleanDocuments(dirtyCount);
    m_statusMessage               = "Closed " + std::to_string(closedCount) + " clean document(s).";
    if (dirtyCount > 0)
        m_statusMessage += " " + std::to_string(dirtyCount) + " dirty document(s) kept open.";
}
}  // namespace editor
