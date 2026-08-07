#pragma once

#include "modules/IEditorModule.h"
#include "scene/SceneTypes.h"

#include <string>

namespace editor
{
class EditorSettingsModule final : public IEditorModule
{
public:
    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

private:
    enum class Category
    {
        Workspace,
        Editor,
        ContentView,
        SceneDefaults,
    };

    std::string m_workingDirectoryInput;
    std::string m_editorFontPathInput;
    std::string m_workspaceStatusMessage;
    std::string m_editorStatusMessage;
    std::string m_contentViewStatusMessage;
    std::string m_sceneDefaultsStatusMessage;
    SceneColor m_objectNoteColor;
    SceneColor m_objectNoteOutlineColor;
    SceneColor m_shapeFillColor;
    SceneColor m_shapeStrokeColor;
    float m_editorFontSizeInput = 16.0f;
    float m_objectNoteFontSizeInput = 16.0f;
    float m_objectNoteOutlineSizeInput = 2.0f;
    bool m_objectNoteOutlineEnabledInput = true;
    bool m_workingDirectoryInitialized = false;
    bool m_editorFontInitialized = false;
    bool m_sceneDefaultsInitialized = false;
    Category m_selectedCategory = Category::Workspace;
};
}  // namespace editor
