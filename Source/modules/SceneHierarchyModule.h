#pragma once

#include "modules/PanelModule.h"
#include "scene/SceneDocument.h"
#include "scene/SceneTypes.h"

#include <filesystem>
#include <string>

namespace editor
{
class SceneHierarchyModule : public PanelModule
{
public:
    SceneHierarchyModule();

private:
    void drawContent(EditorContext& context) override;
    void drawSceneDocument(EditorContext& context, SceneDocument& document, const std::filesystem::path& documentKey);
    void drawSceneNode(EditorContext& context,
                       SceneDocument& document,
                       const std::filesystem::path& documentKey,
                       SceneNode& node,
                       bool isRoot,
                       std::string& pendingDeleteNodeId,
                       std::string& pendingReparentNodeId,
                       std::string& pendingReparentParentId,
                       std::string& pendingMoveNodeId,
                       SceneNodeSiblingMove& pendingMove,
                       std::string& pendingPasteParentId);
};
}  // namespace editor
