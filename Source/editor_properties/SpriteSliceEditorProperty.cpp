#include "editor_properties/SpriteSliceEditorProperty.h"

#include "imgui.h"
#include "scene/node_editors/SpriteSliceUtils.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace editor
{
namespace
{
constexpr float kPreviewLineHitDistance = 7.0f;

enum SlicePreviewHandle
{
    kPreviewHandleLeft = 0,
    kPreviewHandleRight = 1,
    kPreviewHandleTop = 2,
    kPreviewHandleBottom = 3
};

struct SlicePreviewGeometry
{
    ImVec2 start;
    ImVec2 end;
    ImVec2 size;
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    bool hovered = false;
};

SlicePreviewGeometry drawSlicePreview(const SceneNode& node, const ImVec2& size)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##slicePreview", size);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 end(start.x + size.x, start.y + size.y);
    const SpriteSliceMargins margins = spriteSliceNormalizedMargins(node.sprite);

    drawList->AddRectFilled(start, end, IM_COL32(34, 38, 45, 255), 3.0f);
    drawList->AddRect(start, end, IM_COL32(112, 124, 138, 255), 3.0f);

    const float left = start.x + margins.left * size.x;
    const float right = end.x - margins.right * size.x;
    const float top = start.y + margins.top * size.y;
    const float bottom = end.y - margins.bottom * size.y;

    const ImU32 lineColor = IM_COL32(255, 204, 64, 255);
    drawList->AddLine(ImVec2(left, start.y), ImVec2(left, end.y), lineColor, 1.5f);
    drawList->AddLine(ImVec2(right, start.y), ImVec2(right, end.y), lineColor, 1.5f);
    drawList->AddLine(ImVec2(start.x, top), ImVec2(end.x, top), lineColor, 1.5f);
    drawList->AddLine(ImVec2(start.x, bottom), ImVec2(end.x, bottom), lineColor, 1.5f);

    drawList->AddRectFilled(ImVec2(left, top), ImVec2(right, bottom), IM_COL32(255, 204, 64, 34));

    const ImU32 handleColor = IM_COL32(90, 210, 255, 255);
    drawList->AddCircleFilled(ImVec2(left, (start.y + end.y) * 0.5f), 4.0f, handleColor, 12);
    drawList->AddCircleFilled(ImVec2(right, (start.y + end.y) * 0.5f), 4.0f, handleColor, 12);
    drawList->AddCircleFilled(ImVec2((start.x + end.x) * 0.5f, top), 4.0f, handleColor, 12);
    drawList->AddCircleFilled(ImVec2((start.x + end.x) * 0.5f, bottom), 4.0f, handleColor, 12);

    return {start, end, size, left, right, top, bottom, ImGui::IsItemHovered()};
}

int hitPreviewHandle(const SlicePreviewGeometry& preview)
{
    if (!preview.hovered)
        return -1;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    int bestHandle = -1;
    float bestDistance = kPreviewLineHitDistance;

    auto test = [&](int handle, float distance) {
        if (distance <= bestDistance)
        {
            bestHandle = handle;
            bestDistance = distance;
        }
    };

    test(kPreviewHandleLeft, std::abs(mouse.x - preview.left));
    test(kPreviewHandleRight, std::abs(mouse.x - preview.right));
    test(kPreviewHandleTop, std::abs(mouse.y - preview.top));
    test(kPreviewHandleBottom, std::abs(mouse.y - preview.bottom));
    return bestHandle;
}

std::string previewEditKey(const std::string& id, int handle)
{
    return id + ".preview." + std::to_string(handle);
}

int activePreviewHandle(const std::string& id, const std::string& activeEditKey)
{
    const std::string prefix = id + ".preview.";
    if (activeEditKey.rfind(prefix, 0) != 0 || activeEditKey.size() <= prefix.size())
        return -1;
    const char value = activeEditKey[prefix.size()];
    if (value < '0' || value > '3')
        return -1;
    return value - '0';
}

void applyPreviewDrag(EditorPropertyContext& context, const SlicePreviewGeometry& preview, int handle)
{
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    SpriteSliceMargins margins = spriteSliceNormalizedMargins(context.node.sprite);
    switch (handle)
    {
    case kPreviewHandleLeft:
        margins.left = (mouse.x - preview.start.x) / std::max(1.0f, preview.size.x);
        break;
    case kPreviewHandleRight:
        margins.right = (preview.end.x - mouse.x) / std::max(1.0f, preview.size.x);
        break;
    case kPreviewHandleTop:
        margins.top = (mouse.y - preview.start.y) / std::max(1.0f, preview.size.y);
        break;
    case kPreviewHandleBottom:
        margins.bottom = (preview.end.y - mouse.y) / std::max(1.0f, preview.size.y);
        break;
    default:
        return;
    }

    setSpriteSliceNormalizedMargins(context.node.sprite, margins);
    context.document.markDirty();
}

void handlePreviewInteraction(EditorPropertyContext& context, const SlicePreviewGeometry& preview, const std::string& id)
{
    const int activeHandle = activePreviewHandle(id, context.activeEditKey);
    if (activeHandle >= 0)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            applyPreviewDrag(context, preview, activeHandle);
        }
        else
        {
            context.document.commitUndoTransaction();
            context.activeEditKey.clear();
        }
        ImGui::SetMouseCursor(activeHandle < 2 ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        return;
    }

    const int hoveredHandle = hitPreviewHandle(preview);
    if (hoveredHandle >= 0)
    {
        ImGui::SetMouseCursor(hoveredHandle < 2 ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (!context.activeEditKey.empty() && !context.document.hasActiveUndoTransaction())
                context.activeEditKey.clear();
            if (context.activeEditKey.empty())
            {
                context.document.beginUndoTransaction();
                context.activeEditKey = previewEditKey(id, hoveredHandle);
                applyPreviewDrag(context, preview, hoveredHandle);
            }
        }
    }
}

bool drawMarginField(const char* label, float& value)
{
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::InputFloat(label, &value, 1.0f, 10.0f, "%.2f");
    if (changed)
        value = std::max(0.0f, value);
    return changed;
}

bool editMarginField(EditorPropertyContext& context,
                     const std::string& key,
                     const char* label,
                     SpriteSliceMargins& margins,
                     float SpriteSliceMargins::*member)
{
    bool prepared = false;
    if (!context.activeEditKey.empty() && !context.document.hasActiveUndoTransaction())
        context.activeEditKey.clear();
    if (context.activeEditKey.empty())
        prepared = context.document.beginUndoTransaction();

    float value = margins.*member;
    const bool changed = drawMarginField(label, value);
    if (changed)
    {
        margins.*member = value;
        setSpriteSlicePixelMargins(context.node, margins);
        context.document.markDirty();
    }

    if ((ImGui::IsItemActivated() || changed) && context.activeEditKey.empty())
        context.activeEditKey = key;

    const bool ownsEdit = context.activeEditKey == key;
    if (ownsEdit && (ImGui::IsItemDeactivated() || (changed && !ImGui::IsItemActive())))
    {
        context.document.commitUndoTransaction();
        context.activeEditKey.clear();
    }
    else if (prepared && context.activeEditKey.empty())
    {
        context.document.cancelUndoTransaction();
    }
    return changed;
}
}  // namespace

SpriteSliceEditorProperty::SpriteSliceEditorProperty(std::string id, std::string label)
    : EditorProperty(std::move(id), std::move(label))
{}

void SpriteSliceEditorProperty::draw(EditorPropertyContext& context)
{
    ImGui::TextUnformatted(label().c_str());

    const SceneSize referenceSize = spriteSliceReferenceSize(context.node);
    const bool hasSourceSize = spriteSliceHasSourceReferenceSize(context.node);
    ImGui::TextDisabled("Reference: %.0f x %.0f px%s",
                        referenceSize.width,
                        referenceSize.height,
                        hasSourceSize ? "" : " (node size)");

    const float availableWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x);
    const float previewWidth = std::min(availableWidth, 220.0f);
    const float aspect = referenceSize.width > 0.0f ? referenceSize.height / referenceSize.width : 0.6f;
    const float previewHeight = std::clamp(previewWidth * aspect, 80.0f, 150.0f);
    ImGui::PushID((id() + ".preview").c_str());
    const SlicePreviewGeometry preview = drawSlicePreview(context.node, ImVec2(previewWidth, previewHeight));
    handlePreviewInteraction(context, preview, id());
    ImGui::PopID();

    SpriteSliceMargins margins = spriteSlicePixelMargins(context.node);
    ImGui::PushID(id().c_str());
    editMarginField(context, id() + ".left", "Left", margins, &SpriteSliceMargins::left);
    editMarginField(context, id() + ".right", "Right", margins, &SpriteSliceMargins::right);
    editMarginField(context, id() + ".top", "Top", margins, &SpriteSliceMargins::top);
    editMarginField(context, id() + ".bottom", "Bottom", margins, &SpriteSliceMargins::bottom);
    ImGui::PopID();
}
}  // namespace editor
