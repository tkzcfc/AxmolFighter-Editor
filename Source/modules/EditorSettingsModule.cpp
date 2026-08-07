#include "modules/EditorSettingsModule.h"

#include "core/ApplicationRestartService.h"
#include "core/EditorContext.h"
#include "core/EditorPreferencesService.h"
#include "core/ProjectSettings.h"
#include "modules/PanelVisibilityService.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "platform/FileUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace editor
{
namespace
{
constexpr const char* kEditorSettingsPanelId = "Editor Settings";

void restartEditor(EditorContext& context, std::string& statusMessage)
{
    if (auto restartService = context.services().get<ApplicationRestartService>())
    {
        std::string message;
        if (!restartService->restart(&message))
            statusMessage = message.empty() ? "Failed to restart editor." : message;
        return;
    }
    statusMessage = "Application restart service is not available.";
}

std::string normalizedPathString(const std::string& path)
{
    if (path.empty())
        return {};
    return std::filesystem::path(path).lexically_normal().generic_string();
}

std::string lowercaseAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool hasSupportedFontExtension(const std::string& path)
{
    const std::string extension = lowercaseAscii(std::filesystem::path(path).extension().generic_string());
    return extension == ".ttf" || extension == ".ttc" || extension == ".otf";
}

bool fontFileExists(const std::string& path)
{
    if (path.empty())
        return false;
    if (ax::FileUtils* fileUtils = ax::FileUtils::getInstance())
        return fileUtils->isFileExist(path);

    std::error_code error;
    return std::filesystem::exists(path, error) && std::filesystem::is_regular_file(path, error);
}

bool editColor(const char* label, SceneColor& color)
{
    float data[4] = {color.r, color.g, color.b, color.a};
    if (!ImGui::ColorEdit4(label, data))
        return false;
    color = {data[0], data[1], data[2], data[3]};
    return true;
}
}

void EditorSettingsModule::onAttach(EditorContext&) {}

void EditorSettingsModule::onDetach(EditorContext&) {}

void EditorSettingsModule::onUpdate(EditorContext&, float) {}

void EditorSettingsModule::onImGuiRender(EditorContext& context)
{
    auto visibility = context.services().get<PanelVisibilityService>();
    if (visibility && !visibility->isOpen(kEditorSettingsPanelId))
        return;

    ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin(kEditorSettingsPanelId, &open))
    {
        ImGui::End();
        if (!open && visibility)
            visibility->close(kEditorSettingsPanelId);
        return;
    }

    if (auto preferences = context.services().get<EditorPreferencesService>())
    {
        if (!m_workingDirectoryInitialized)
        {
            const std::string overridePath = preferences->workingDirectoryOverride();
            m_workingDirectoryInput = overridePath.empty() ? context.settings().resourceRoot() : overridePath;
            m_workingDirectoryInitialized = true;
        }
        if (!m_editorFontInitialized)
        {
            m_editorFontPathInput = preferences->editorFontPath();
            m_editorFontSizeInput = preferences->editorFontSize();
            m_editorFontInitialized = true;
        }
        if (!m_sceneDefaultsInitialized)
        {
            m_objectNoteColor = preferences->objectNoteColor();
            m_objectNoteFontSizeInput = preferences->objectNoteFontSize();
            m_objectNoteOutlineEnabledInput = preferences->objectNoteOutlineEnabled();
            m_objectNoteOutlineColor = preferences->objectNoteOutlineColor();
            m_objectNoteOutlineSizeInput = preferences->objectNoteOutlineSize();
            m_shapeFillColor = preferences->shapeFillColor();
            m_shapeStrokeColor = preferences->shapeStrokeColor();
            m_sceneDefaultsInitialized = true;
        }

        constexpr float kCategoryWidth = 160.0f;
        if (ImGui::BeginChild("SettingsCategories", ImVec2(kCategoryWidth, 0.0f), true))
        {
            if (ImGui::Selectable("Workspace", m_selectedCategory == Category::Workspace))
                m_selectedCategory = Category::Workspace;
            if (ImGui::Selectable("Editor", m_selectedCategory == Category::Editor))
                m_selectedCategory = Category::Editor;
            if (ImGui::Selectable("Content View", m_selectedCategory == Category::ContentView))
                m_selectedCategory = Category::ContentView;
            if (ImGui::Selectable("Scene Defaults", m_selectedCategory == Category::SceneDefaults))
                m_selectedCategory = Category::SceneDefaults;
        }
        ImGui::EndChild();
        ImGui::SameLine();

        if (ImGui::BeginChild("SettingsContent", ImVec2(0.0f, 0.0f), false))
        {
            if (m_selectedCategory == Category::Workspace)
            {
                ImGui::TextUnformatted("Workspace");
                ImGui::Separator();
                ImGui::TextDisabled("Current: %s", context.settings().resourceRoot().c_str());
                ImGui::InputText("Working Directory", &m_workingDirectoryInput);
                if (ImGui::Button("Apply and Restart"))
                {
                    const std::filesystem::path requested(m_workingDirectoryInput);
                    if (m_workingDirectoryInput.empty())
                    {
                        preferences->clearWorkingDirectoryOverride();
                        restartEditor(context, m_workspaceStatusMessage);
                    }
                    else
                    {
                        std::error_code error;
                        if (std::filesystem::exists(requested, error) && std::filesystem::is_directory(requested, error))
                        {
                            preferences->setWorkingDirectoryOverride(requested.lexically_normal().generic_string());
                            restartEditor(context, m_workspaceStatusMessage);
                        }
                        else
                        {
                            m_workspaceStatusMessage = "Working directory does not exist.";
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset to Default"))
                {
                    preferences->clearWorkingDirectoryOverride();
                    restartEditor(context, m_workspaceStatusMessage);
                }
                if (!m_workspaceStatusMessage.empty())
                    ImGui::TextDisabled("%s", m_workspaceStatusMessage.c_str());
            }
            else if (m_selectedCategory == Category::Editor)
            {
                ImGui::TextUnformatted("Editor");
                ImGui::Separator();
                ImGui::InputText("Default Font", &m_editorFontPathInput);
                ImGui::InputFloat("Default Font Size", &m_editorFontSizeInput, 1.0f, 2.0f, "%.1f");
                if (ImGui::Button("Save Font Settings"))
                {
                    const std::string requestedFontPath = normalizedPathString(m_editorFontPathInput);
                    if (!hasSupportedFontExtension(requestedFontPath))
                    {
                        m_editorStatusMessage = "Font must be a .ttf, .ttc, or .otf file.";
                    }
                    else if (!fontFileExists(requestedFontPath))
                    {
                        m_editorStatusMessage = "Font file does not exist.";
                    }
                    else
                    {
                        preferences->setEditorFontPath(requestedFontPath);
                        preferences->setEditorFontSize(m_editorFontSizeInput);
                        m_editorFontPathInput = preferences->editorFontPath();
                        m_editorFontSizeInput = preferences->editorFontSize();
                        m_editorStatusMessage = "Font settings saved. Restart editor to apply.";
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Font to Default"))
                {
                    preferences->clearEditorFontPath();
                    preferences->clearEditorFontSize();
                    m_editorFontPathInput = preferences->editorFontPath();
                    m_editorFontSizeInput = preferences->editorFontSize();
                    m_editorStatusMessage = "Font settings reset. Restart editor to apply.";
                }
                if (!m_editorStatusMessage.empty())
                    ImGui::TextDisabled("%s", m_editorStatusMessage.c_str());
            }
            else if (m_selectedCategory == Category::ContentView)
            {
                ImGui::TextUnformatted("Content View");
                ImGui::Separator();
                bool showRootBounds = preferences->showRootBounds();
                if (ImGui::Checkbox("Show Root Bounds", &showRootBounds))
                {
                    preferences->setShowRootBounds(showRootBounds);
                    m_contentViewStatusMessage = "Content view settings saved.";
                }

                SceneColor color = preferences->rootBoundsColor();
                if (editColor("Root Bounds Color", color))
                {
                    preferences->setRootBoundsColor(color);
                    m_contentViewStatusMessage = "Content view settings saved.";
                }
                if (!m_contentViewStatusMessage.empty())
                    ImGui::TextDisabled("%s", m_contentViewStatusMessage.c_str());
            }
            else
            {
                ImGui::TextUnformatted("Scene Defaults");
                ImGui::Separator();
                ImGui::TextUnformatted("Object Note");
                ImGui::InputFloat("Note Font Size", &m_objectNoteFontSizeInput, 1.0f, 2.0f, "%.1f");
                editColor("Note Color", m_objectNoteColor);
                ImGui::Checkbox("Note Outline", &m_objectNoteOutlineEnabledInput);
                editColor("Note Outline Color", m_objectNoteOutlineColor);
                ImGui::InputFloat("Note Outline Size", &m_objectNoteOutlineSizeInput, 0.5f, 1.0f, "%.1f");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Shape");
                editColor("Fill Color", m_shapeFillColor);
                editColor("Stroke Color", m_shapeStrokeColor);

                if (ImGui::Button("Save Scene Defaults"))
                {
                    preferences->setObjectNoteColor(m_objectNoteColor);
                    preferences->setObjectNoteFontSize(m_objectNoteFontSizeInput);
                    preferences->setObjectNoteOutlineEnabled(m_objectNoteOutlineEnabledInput);
                    preferences->setObjectNoteOutlineColor(m_objectNoteOutlineColor);
                    preferences->setObjectNoteOutlineSize(m_objectNoteOutlineSizeInput);
                    preferences->setShapeFillColor(m_shapeFillColor);
                    preferences->setShapeStrokeColor(m_shapeStrokeColor);
                    m_objectNoteColor = preferences->objectNoteColor();
                    m_objectNoteFontSizeInput = preferences->objectNoteFontSize();
                    m_objectNoteOutlineEnabledInput = preferences->objectNoteOutlineEnabled();
                    m_objectNoteOutlineColor = preferences->objectNoteOutlineColor();
                    m_objectNoteOutlineSizeInput = preferences->objectNoteOutlineSize();
                    m_shapeFillColor = preferences->shapeFillColor();
                    m_shapeStrokeColor = preferences->shapeStrokeColor();
                    m_sceneDefaultsStatusMessage = "Scene defaults saved. New Object nodes and shapes will use these values.";
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Scene Defaults"))
                {
                    preferences->clearObjectNoteColor();
                    preferences->clearObjectNoteFontSize();
                    preferences->clearObjectNoteOutlineEnabled();
                    preferences->clearObjectNoteOutlineColor();
                    preferences->clearObjectNoteOutlineSize();
                    preferences->clearShapeFillColor();
                    preferences->clearShapeStrokeColor();
                    m_objectNoteColor = preferences->objectNoteColor();
                    m_objectNoteFontSizeInput = preferences->objectNoteFontSize();
                    m_objectNoteOutlineEnabledInput = preferences->objectNoteOutlineEnabled();
                    m_objectNoteOutlineColor = preferences->objectNoteOutlineColor();
                    m_objectNoteOutlineSizeInput = preferences->objectNoteOutlineSize();
                    m_shapeFillColor = preferences->shapeFillColor();
                    m_shapeStrokeColor = preferences->shapeStrokeColor();
                    m_sceneDefaultsStatusMessage = "Scene defaults reset. New Object nodes and shapes will use default values.";
                }
                if (!m_sceneDefaultsStatusMessage.empty())
                    ImGui::TextDisabled("%s", m_sceneDefaultsStatusMessage.c_str());
            }
        }
        ImGui::EndChild();
    }
    else
    {
        ImGui::TextDisabled("Editor preferences service is not available.");
    }

    ImGui::End();
    if (!open && visibility)
        visibility->close(kEditorSettingsPanelId);
}
}  // namespace editor
