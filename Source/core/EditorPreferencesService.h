#pragma once

#include "core/IService.h"
#include "scene/SceneTypes.h"

#include <string>
#include <vector>

namespace editor
{
class ProjectSettings;

struct EditorPreferences
{
    struct Workspace
    {
        std::string workingDirectoryOverride;
    };

    struct NpkExporter
    {
        std::string npkDirectory;
        std::string outputRoot;
        std::string imgFilter;
    };

    struct Editor
    {
        std::string fontPath;
        float fontSize = 16.0f;
    };

    struct ContentView
    {
        bool showRootBounds = true;
        SceneColor rootBoundsColor = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct ObjectNoteDefaults
    {
        SceneColor color = {1.0f, 1.0f, 1.0f, 1.0f};
        float fontSize = 16.0f;
        bool outlineEnabled = true;
        SceneColor outlineColor = {0.0f, 0.0f, 0.0f, 1.0f};
        float outlineSize = 2.0f;
    };

    struct ShapeDefaults
    {
        SceneColor fillColor = {1.0f, 1.0f, 1.0f, 1.0f};
        SceneColor strokeColor = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct SceneDefaults
    {
        ObjectNoteDefaults objectNote;
        ShapeDefaults shape;
    };

    Workspace workspace;
    NpkExporter npkExporter;
    Editor editor;
    ContentView contentView;
    SceneDefaults sceneDefaults;
};

struct AssetBrowserEntry
{
    bool rootOpen = true;
    std::vector<std::string> openPaths;
};

EditorPreferences defaultEditorPreferences();

class EditorPreferencesService final : public IService
{
public:
    bool showRootBounds() const;
    void setShowRootBounds(bool value);

    SceneColor rootBoundsColor() const;
    void setRootBoundsColor(const SceneColor& value);

    std::string editorFontPath() const;
    void setEditorFontPath(const std::string& path);
    void clearEditorFontPath();

    float editorFontSize() const;
    void setEditorFontSize(float size);
    void clearEditorFontSize();

    SceneColor objectNoteColor() const;
    void setObjectNoteColor(const SceneColor& value);
    void clearObjectNoteColor();

    float objectNoteFontSize() const;
    void setObjectNoteFontSize(float size);
    void clearObjectNoteFontSize();

    bool objectNoteOutlineEnabled() const;
    void setObjectNoteOutlineEnabled(bool value);
    void clearObjectNoteOutlineEnabled();

    SceneColor objectNoteOutlineColor() const;
    void setObjectNoteOutlineColor(const SceneColor& value);
    void clearObjectNoteOutlineColor();

    float objectNoteOutlineSize() const;
    void setObjectNoteOutlineSize(float size);
    void clearObjectNoteOutlineSize();

    SceneColor shapeFillColor() const;
    void setShapeFillColor(const SceneColor& value);
    void clearShapeFillColor();

    SceneColor shapeStrokeColor() const;
    void setShapeStrokeColor(const SceneColor& value);
    void clearShapeStrokeColor();

    std::string workingDirectoryOverride() const;
    void setWorkingDirectoryOverride(const std::string& path);
    void clearWorkingDirectoryOverride();
    void applyWorkingDirectory(ProjectSettings& settings) const;

    std::string npkExporterDirectory() const;
    void setNpkExporterDirectory(const std::string& path);
    std::string npkExporterOutputRoot() const;
    void setNpkExporterOutputRoot(const std::string& path);
    std::string npkExporterFilter() const;
    void setNpkExporterFilter(const std::string& filter);

    AssetBrowserEntry assetBrowserEntry(const std::string& root) const;
    void setAssetBrowserEntry(const std::string& root, const AssetBrowserEntry& entry);

private:
    EditorPreferences preferences() const;
    EditorPreferences loadPreferences() const;
    void savePreferences(const EditorPreferences& value) const;
};
}  // namespace editor
