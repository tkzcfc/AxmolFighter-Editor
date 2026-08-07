#include "app/EditorRuntime.h"

#include "asset_browser/AssetBrowserService.h"
#include "asset_browser/AssetSelectionService.h"
#include "core/ApplicationRestartService.h"
#include "core/EditorLogService.h"
#include "core/EditorPreferencesService.h"
#include "core/LayoutService.h"
#include "core/ModalDialogService.h"
#include "document_editors/BoxDocumentEditor.h"
#include "document_editors/DocumentEditorRegistry.h"
#include "document_editors/FrameAnimationDocumentEditor.h"
#include "document_editors/MotionDocumentEditor.h"
#include "documents/DocumentCloseService.h"
#include "modules/AssetBrowserModule.h"
#include "modules/AssetPreviewModule.h"
#include "modules/ContentModule.h"
#include "modules/DocumentCloseDialogModule.h"
#include "modules/DockingLayoutModule.h"
#include "modules/EditorSettingsModule.h"
#include "modules/InspectorModule.h"
#include "modules/LogModule.h"
#include "modules/MainMenuModule.h"
#include "modules/ModalDialogModule.h"
#include "modules/NpkExporterModule.h"
#include "combat/BoxEditorSessionService.h"
#include "modules/DocumentEditorPanelsModule.h"
#include "modules/PanelVisibilityService.h"
#include "modules/SceneHierarchyModule.h"
#include "modules/ShapeListModule.h"
#include "scene/SceneNodeClipboardService.h"

#include <memory>
#include <stdexcept>

namespace editor
{
void EditorRuntime::initialize()
{
    if (m_initialized)
        return;

    m_context.services().add(std::make_shared<LayoutService>());
    m_context.services().add(std::make_shared<PanelVisibilityService>());
    m_context.services().add(std::make_shared<ApplicationRestartService>());
    auto preferences = std::make_shared<EditorPreferencesService>();
    m_context.services().add(preferences);
    preferences->applyWorkingDirectory(m_context.settings());
    m_context.services().add(std::make_shared<EditorLogService>());
    m_context.services().add(std::make_shared<DocumentCloseService>());
    m_context.services().add(std::make_shared<ModalDialogService>());
    m_context.services().add(std::make_shared<AssetBrowserService>());
    m_context.services().add(std::make_shared<AssetSelectionService>());
    m_context.services().add(std::make_shared<BoxEditorSessionService>());
    auto documentEditors = std::make_shared<DocumentEditorRegistry>();
    std::string registrationError;
    if (!documentEditors->registerEditor(std::make_unique<FrameAnimationDocumentEditor>(), registrationError) ||
        !documentEditors->registerEditor(std::make_unique<BoxDocumentEditor>(), registrationError) ||
        !documentEditors->registerEditor(std::make_unique<MotionDocumentEditor>(), registrationError))
        throw std::runtime_error("Failed to register document editor: " + registrationError);
    m_context.services().add(documentEditors);
    m_context.services().add(std::make_shared<SceneNodeClipboardService>());
    registerDefaultModules();
    m_context.modules().attachAll(m_context);
    m_initialized = true;
}

void EditorRuntime::shutdown()
{
    if (!m_initialized)
        return;

    m_context.modules().detachAll(m_context);
    m_initialized = false;
}

void EditorRuntime::update(float deltaTime)
{
    m_context.modules().updateAll(m_context, deltaTime);
    m_context.tools().updateActive(m_context, deltaTime);
}

void EditorRuntime::renderImGui()
{
    m_context.modules().renderAll(m_context);
    m_context.tools().renderActive(m_context);
}

EditorContext& EditorRuntime::context()
{
    return m_context;
}

void EditorRuntime::registerDefaultModules()
{
    if (m_modulesRegistered)
        return;

    auto& modules = m_context.modules();
    modules.add(std::make_unique<MainMenuModule>());
    modules.add(std::make_unique<NpkExporterModule>());
    modules.add(std::make_unique<DockingLayoutModule>());
    modules.add(std::make_unique<DocumentEditorPanelsModule>());
    modules.add(std::make_unique<LogModule>());
    modules.add(std::make_unique<EditorSettingsModule>());
    modules.add(std::make_unique<ContentModule>());
    modules.add(std::make_unique<InspectorModule>());
    modules.add(std::make_unique<AssetBrowserModule>());
    modules.add(std::make_unique<AssetPreviewModule>());
    modules.add(std::make_unique<SceneHierarchyModule>());
    modules.add(std::make_unique<ShapeListModule>());
    modules.add(std::make_unique<DocumentCloseDialogModule>());
    modules.add(std::make_unique<ModalDialogModule>());

    m_modulesRegistered = true;
}
}  // namespace editor
