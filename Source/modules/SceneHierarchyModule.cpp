#include "modules/SceneHierarchyModule.h"

#include "core/EditorContext.h"
#include "documents/IEditorDocument.h"
#include "scene/node_editors/SceneNodeEditor.h"
#include "imgui.h"
#include "scene/SceneNodeClipboardService.h"
#include "ui/EditorIcons.h"

#include <string>

namespace editor
{
namespace
{
constexpr ImVec4 kHierarchyNoteColor = ImVec4(0.98f, 0.78f, 0.35f, 1.0f);
}  // namespace

SceneHierarchyModule::SceneHierarchyModule() : PanelModule("Scene Hierarchy") {}

void SceneHierarchyModule::drawContent(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    if (!document)
    {
        ImGui::TextDisabled("No scene loaded.");
        return;
    }

    if (auto* sceneDocument = dynamic_cast<SceneDocument*>(document))
    {
        drawSceneDocument(context, *sceneDocument, document->path());
        return;
    }

    if (ImGui::TreeNodeEx(document->getDisplayName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Add editor-specific hierarchy nodes here.");
        ImGui::TreePop();
    }
}

void SceneHierarchyModule::drawSceneDocument(EditorContext& context,
                                             SceneDocument& document,
                                             const std::filesystem::path& documentKey)
{
    ImGui::Text("Render Tree");
    ImGui::Separator();
    std::string pendingDeleteNodeId;
    std::string pendingReparentNodeId;
    std::string pendingReparentParentId;
    std::string pendingMoveNodeId;
    std::string pendingPasteParentId;
    SceneNodeSiblingMove pendingMove = SceneNodeSiblingMove::Up;
    const bool windowActive          = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
                              ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    if (windowActive && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::IsAnyItemActive())
    {
        pendingDeleteNodeId = document.selectedNodeId();
    }
    if (windowActive && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive())
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl)
        {
            if (auto clipboard = context.services().get<SceneNodeClipboardService>())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_C, false))
                    clipboard->copySelectedNode(document, documentKey);
                else if (ImGui::IsKeyPressed(ImGuiKey_V, false) && !document.hasActiveUndoTransaction())
                    clipboard->pasteNode(document, documentKey);
            }
        }
    }
    drawSceneNode(context, document, documentKey, document.root(), true, pendingDeleteNodeId, pendingReparentNodeId,
                  pendingReparentParentId, pendingMoveNodeId, pendingMove, pendingPasteParentId);
    if (!pendingReparentNodeId.empty())
        document.reparentNode(pendingReparentNodeId, pendingReparentParentId);
    else if (!pendingMoveNodeId.empty())
        document.moveNodeSiblingOrder(pendingMoveNodeId, pendingMove);
    else if (!pendingDeleteNodeId.empty())
        document.deleteNode(pendingDeleteNodeId);
    else if (!pendingPasteParentId.empty())
    {
        if (auto clipboard = context.services().get<SceneNodeClipboardService>())
            clipboard->pasteNode(document, documentKey, pendingPasteParentId);
    }
}

void SceneHierarchyModule::drawSceneNode(EditorContext& context,
                                         SceneDocument& document,
                                         const std::filesystem::path& documentKey,
                                         SceneNode& node,
                                         bool isRoot,
                                         std::string& pendingDeleteNodeId,
                                         std::string& pendingReparentNodeId,
                                         std::string& pendingReparentParentId,
                                         std::string& pendingMoveNodeId,
                                         SceneNodeSiblingMove& pendingMove,
                                         std::string& pendingPasteParentId)
{
    ImGui::PushID(node.id.c_str());

    bool visible = node.visible;
    drawEditorIconToggle(visible ? kEditorIconEye : kEditorIconEyeSlash, visible ? "Visible" : "Hidden", visible);
    if (visible != node.visible)
    {
        document.beginUndoTransaction();
        node.visible = visible;
        document.markDirty();
        document.commitUndoTransaction();
    }
    ImGui::SameLine(0.0f, 4.0f);

    bool locked = node.locked;
    drawEditorIconToggle(locked ? kEditorIconLock : kEditorIconUnlock, locked ? "Locked" : "Unlocked", locked);
    if (locked != node.locked)
    {
        document.beginUndoTransaction();
        node.locked = locked;
        document.markDirty();
        document.commitUndoTransaction();
    }
    ImGui::SameLine(0.0f, 4.0f);

    const bool hadChildren = !node.children.empty();
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hadChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (document.selectedNodeId() == node.id)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (isRoot)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    const bool open = ImGui::TreeNodeEx("##Node", flags, "%s", node.name.c_str());
    if (!node.note.empty())
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 itemMin    = ImGui::GetItemRectMin();
        const float labelX      = itemMin.x + ImGui::GetTreeNodeToLabelSpacing();
        const float labelY      = itemMin.y + style.FramePadding.y;
        const ImVec2 nameSize   = ImGui::CalcTextSize(node.name.c_str(), nullptr, true);
        const std::string noteText = "(" + node.note + ")";
        ImGui::GetWindowDrawList()->AddText(ImVec2(labelX + nameSize.x, labelY),
                                            ImGui::ColorConvertFloat4ToU32(kHierarchyNoteColor),
                                            noteText.c_str());
    }
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        document.selectNode(node.id);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        document.selectNode(node.id);

    if (!isRoot && ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("LAYER_NODE", node.id.c_str(), node.id.size() + 1);
        ImGui::TextUnformatted(node.name.c_str());
        if (!node.note.empty())
        {
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(kHierarchyNoteColor, "(%s)", node.note.c_str());
        }
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("LAYER_NODE", ImGuiDragDropFlags_AcceptBeforeDelivery);
        if (payload)
        {
            const char* draggedId           = static_cast<const char*>(payload->Data);
            const std::string draggedNodeId = draggedId ? draggedId : "";
            if (document.canReparent(draggedNodeId, node.id))
            {
                if (payload->IsDelivery())
                {
                    pendingReparentNodeId   = draggedNodeId;
                    pendingReparentParentId = node.id;
                }
            }
            else
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem("NodeContextMenu"))
    {
        auto clipboard = context.services().get<SceneNodeClipboardService>();

        if (document.canAddChild(node.id) && ImGui::BeginMenu("Add Child"))
        {
            for (const std::unique_ptr<SceneNodeEditor>& editor : SceneNodeEditorRegistry::instance().editors())
            {
                if (ImGui::MenuItem(std::string(editor->displayName()).c_str()))
                    document.addChildOfType(node.id, std::string(editor->typeId()));
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, clipboard && node.id != "root"))
            clipboard->copyNode(document, documentKey, node.id);
        const bool canPaste = clipboard && clipboard->hasNode() && document.canAddChild(node.id) &&
                              !document.hasActiveUndoTransaction();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPaste))
            pendingPasteParentId = node.id;
        ImGui::Separator();
        if (ImGui::MenuItem("Move Up", nullptr, false,
                            document.canMoveNodeSiblingOrder(node.id, SceneNodeSiblingMove::Up)))
        {
            pendingMoveNodeId = node.id;
            pendingMove       = SceneNodeSiblingMove::Up;
        }
        if (ImGui::MenuItem("Move Down", nullptr, false,
                            document.canMoveNodeSiblingOrder(node.id, SceneNodeSiblingMove::Down)))
        {
            pendingMoveNodeId = node.id;
            pendingMove       = SceneNodeSiblingMove::Down;
        }
        if (ImGui::MenuItem("Move To Top", nullptr, false,
                            document.canMoveNodeSiblingOrder(node.id, SceneNodeSiblingMove::Top)))
        {
            pendingMoveNodeId = node.id;
            pendingMove       = SceneNodeSiblingMove::Top;
        }
        if (ImGui::MenuItem("Move To Bottom", nullptr, false,
                            document.canMoveNodeSiblingOrder(node.id, SceneNodeSiblingMove::Bottom)))
        {
            pendingMoveNodeId = node.id;
            pendingMove       = SceneNodeSiblingMove::Bottom;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Delete", false, document.canDeleteNode(node.id)))
            pendingDeleteNodeId = node.id;
        ImGui::EndPopup();
    }

    if (open && hadChildren)
    {
        for (SceneNode& child : node.children)
        {
            drawSceneNode(context, document, documentKey, child, false, pendingDeleteNodeId, pendingReparentNodeId,
                          pendingReparentParentId, pendingMoveNodeId, pendingMove, pendingPasteParentId);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

}  // namespace editor
