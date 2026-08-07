#pragma once

#include "documents/IEditorDocument.h"
#include "layer/ShapePluginData.h"
#include "scene/SceneDocument.h"

#include <string>
#include <vector>

namespace editor
{
class LayerDocument final : public IEditorDocument, public SceneDocument
{
public:
    static constexpr int kCurrentVersion = 2;

    bool open(const std::filesystem::path& path) override;
    bool save() override;
    bool isDirty() const override;
    std::string getDisplayName() const override;
    const std::filesystem::path& path() const override;
    std::string lastError() const override;

    static bool createDefaultFile(const std::filesystem::path& path, std::string& message);

    bool canUndo() const override { return SceneDocument::canUndo(); }
    bool undo() override { return SceneDocument::undo(); }

    bool hasShapeSelection() const;
    const std::string& selectedShapeId() const;
    bool selectShape(const std::string& id);
    void clearShapeSelection();
    LayerShape* selectedShape();
    const LayerShape* selectedShape() const;
    std::vector<LayerShape>& shapes();
    const std::vector<LayerShape>& shapes() const;
    LayerShape* addShape(const std::string& type);
    bool canDeleteShape(const std::string& id) const;
    bool deleteShape(const std::string& id);

private:
    bool loadFromJson(const std::string& json);
    std::string toJson() const;
    void resetToDefault();

    std::string nextShapeId();
    void scanShapeIds();
    std::string nextUniqueShapeName(const std::string& baseName) const;
    void onSceneNodeSelected() override;
    void onSceneNodeSelectionCleared() override;
    std::string captureSceneExtensionJson() const override;
    void restoreSceneExtensionJson(const std::string& json) override;
    void writeSceneExtensionSnapshot(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                     const std::string& extensionJson) const override;
    std::string sceneDirtyExtensionJson(const std::string& extensionJson) const override;

    std::filesystem::path m_path;
    std::string m_selectedShapeId;
    std::vector<LayerShape> m_shapes;
    std::string m_lastError;
    int m_version         = kCurrentVersion;
    int m_nextShapeSerial = 1;
};
}  // namespace editor
