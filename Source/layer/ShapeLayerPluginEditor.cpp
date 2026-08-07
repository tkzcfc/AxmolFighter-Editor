#include "layer/ShapeLayerPluginEditor.h"

#include "core/EditorContext.h"
#include "documents/LayerDocument.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"
#include "ui/EditorIcons.h"

#include <algorithm>
#include <cfloat>
#include <functional>
#include <vector>

namespace editor
{
namespace
{
constexpr const char* kIconDrawPolygon = "\xef\x97\xae";  // fa-draw-polygon
constexpr const char* kValueInt = "Int";
constexpr const char* kValueString = "String";
constexpr const char* kValueFloat = "Float";

const char* shapeTypeLabel(const std::string& type)
{
    if (type == "Circle")
        return "Circle";
    if (type == "Polygon")
        return "Polygon";
    return "Rectangle";
}

std::string normalizeValueType(const std::string& value)
{
    if (value == kValueInt || value == kValueFloat)
        return value;
    return kValueString;
}

const std::vector<std::string>& valueTypeNames()
{
    static const std::vector<std::string> names = {kValueInt, kValueString, kValueFloat};
    return names;
}

std::string nextPropertyKey(const std::vector<ObjectKeyValue>& properties)
{
    auto exists = [&](const std::string& key) {
        return std::any_of(properties.begin(), properties.end(),
                           [&](const ObjectKeyValue& property) { return property.key == key; });
    };
    if (!exists("key"))
        return "key";
    for (int index = 1;; ++index)
    {
        const std::string candidate = "key_" + std::to_string(index);
        if (!exists(candidate))
            return candidate;
    }
}

void commitImmediateEdit(LayerDocument& document, const std::function<void()>& edit)
{
    document.beginUndoTransaction();
    edit();
    document.markDirty();
    document.commitUndoTransaction();
}

void drawShapePropertiesTable(LayerDocument& document, LayerShape& shape)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Shape Properties");
    ImGui::Separator();

    if (ImGui::Button("Add Property"))
    {
        commitImmediateEdit(document, [&]() {
            ObjectKeyValue property;
            property.key = nextPropertyKey(shape.properties);
            shape.properties.push_back(std::move(property));
        });
    }

    if (!ImGui::BeginTable("ShapeProperties", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        return;

    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.35f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableHeadersRow();

    int pendingDeleteIndex = -1;
    for (int index = 0; index < static_cast<int>(shape.properties.size()); ++index)
    {
        ObjectKeyValue& property = shape.properties[index];
        property.type = normalizeValueType(property.type);

        ImGui::PushID(index);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        std::string key = property.key;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##key", &key) && key != property.key)
        {
            commitImmediateEdit(document, [&]() { property.key = key; });
        }

        ImGui::TableSetColumnIndex(1);
        std::string type = property.type;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##type", type.c_str()))
        {
            for (const std::string& item : valueTypeNames())
            {
                const bool selected = type == item;
                if (ImGui::Selectable(item.c_str(), selected))
                {
                    commitImmediateEdit(document, [&]() { property.type = item; });
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (property.type == kValueInt)
        {
            int value = property.intValue;
            if (ImGui::InputInt("##value", &value) && value != property.intValue)
            {
                commitImmediateEdit(document, [&]() { property.intValue = value; });
            }
        }
        else if (property.type == kValueFloat)
        {
            float value = property.floatValue;
            if (ImGui::InputFloat("##value", &value) && value != property.floatValue)
            {
                commitImmediateEdit(document, [&]() { property.floatValue = value; });
            }
        }
        else
        {
            std::string value = property.stringValue;
            if (ImGui::InputText("##value", &value) && value != property.stringValue)
            {
                commitImmediateEdit(document, [&]() { property.stringValue = value; });
            }
        }

        ImGui::TableSetColumnIndex(3);
        if (ImGui::SmallButton("Delete"))
            pendingDeleteIndex = index;

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (pendingDeleteIndex >= 0 && pendingDeleteIndex < static_cast<int>(shape.properties.size()))
    {
        commitImmediateEdit(document, [&]() {
            shape.properties.erase(shape.properties.begin() + pendingDeleteIndex);
        });
    }
}

void editPolygonPoints(LayerDocument& document,
                       LayerShape& shape,
                       std::string& activeEditKey,
                       const std::string& editPrefix)
{
    ImGui::TextUnformatted("Points");
    for (int index = 0; index < static_cast<int>(shape.points.size()); ++index)
    {
        ImGui::PushID(index);
        editVec2(document, activeEditKey, editPrefix + "point." + std::to_string(index),
                 ("Point " + std::to_string(index)).c_str(), shape.points[index]);
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete") && shape.points.size() > 3)
        {
            document.beginUndoTransaction();
            shape.points.erase(shape.points.begin() + index);
            document.markDirty();
            document.commitUndoTransaction();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Add Point"))
    {
        document.beginUndoTransaction();
        if (shape.points.empty())
        {
            shape.points.push_back({0.0f, 0.0f});
        }
        else
        {
            const SceneVec2& first = shape.points.front();
            const SceneVec2& last  = shape.points.back();
            shape.points.push_back({(first.x + last.x) * 0.5f, (first.y + last.y) * 0.5f});
        }
        document.markDirty();
        document.commitUndoTransaction();
    }
}
}  // namespace

std::string_view ShapeLayerPluginEditor::pluginId() const
{
    return "shape";
}

std::string_view ShapeLayerPluginEditor::displayName() const
{
    return "Shape";
}

void ShapeLayerPluginEditor::drawLayerContentToolbar(EditorContext&,
                                                     LayerDocument&,
                                                     SceneCanvasToolbarContext& toolbar) const
{
    toolbar.drawToolButton(kContentToolId, "Shape Edit", kIconDrawPolygon, 0xf5ee,
                           "Edit and drag shapes on the content canvas", "S");
}

bool ShapeLayerPluginEditor::handleLayerCanvasInteraction(EditorContext& context,
                                                          LayerDocument& document,
                                                          const SceneCanvasContext& canvas) const
{
    return m_canvasTool.handleInput(context, document, canvas);
}

void ShapeLayerPluginEditor::drawLayerCanvasOverlay(EditorContext& context,
                                                    LayerDocument& document,
                                                    const SceneCanvasContext& canvas) const
{
    m_canvasTool.drawOverlay(context, document, canvas);
}

void ShapeLayerPluginEditor::drawList(EditorContext&, LayerDocument& document) const
{
    if (ImGui::Button("Rectangle"))
        document.addShape("Rectangle");
    ImGui::SameLine();
    if (ImGui::Button("Circle"))
        document.addShape("Circle");
    ImGui::SameLine();
    if (ImGui::Button("Polygon"))
        document.addShape("Polygon");

    ImGui::Separator();
    std::string pendingDelete;
    for (LayerShape& shape : document.shapes())
    {
        ImGui::PushID(shape.id.c_str());

        bool visible = shape.visible;
        if (drawEditorIconToggle(visible ? kEditorIconEye : kEditorIconEyeSlash, visible ? "Visible" : "Hidden",
                                 visible))
        {
            document.beginUndoTransaction();
            shape.visible = visible;
            document.markDirty();
            document.commitUndoTransaction();
        }
        ImGui::SameLine(0.0f, 4.0f);

        bool locked = shape.locked;
        if (drawEditorIconToggle(locked ? kEditorIconLock : kEditorIconUnlock, locked ? "Locked" : "Unlocked", locked))
        {
            document.beginUndoTransaction();
            shape.locked = locked;
            document.markDirty();
            document.commitUndoTransaction();
        }
        ImGui::SameLine(0.0f, 4.0f);

        const bool selected     = document.selectedShapeId() == shape.id;
        const std::string label = shape.name + "  [" + shapeTypeLabel(shape.type) + "]";
        if (ImGui::Selectable(label.c_str(), selected))
            document.selectShape(shape.id);

        if (ImGui::BeginPopupContextItem("ShapeContextMenu"))
        {
            if (ImGui::MenuItem("Delete"))
                pendingDelete = shape.id;
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (!pendingDelete.empty())
        document.deleteShape(pendingDelete);
}

void ShapeLayerPluginEditor::drawInspector(LayerDocument& document, std::string& activeEditKey) const
{
    LayerShape* shape = document.selectedShape();
    if (!shape)
        return;

    ImGui::Text("Shape");
    ImGui::Separator();

    editBool(document, "Visible", shape->visible);
    editBool(document, "Locked", shape->locked);

    ImGui::BeginDisabled(shape->locked);
    const std::string editPrefix = shape->id + ":shape.";
    editString(document, activeEditKey, editPrefix + "name", "Name", shape->name);
    ImGui::TextDisabled("ID: %s", shape->id.c_str());
    ImGui::TextDisabled("Type: %s", shape->type.c_str());
    ImGui::Separator();
    editVec2(document, activeEditKey, editPrefix + "position", "Position", shape->position);
    editFloat(document, activeEditKey, editPrefix + "rotation", "Rotation", shape->rotation);
    editColor(document, activeEditKey, editPrefix + "color", "Color", shape->color);
    editIntRange(document, activeEditKey, editPrefix + "opacity", "Opacity", shape->opacity, 0, 255);

    if (shape->type == "Circle")
    {
        editFloat(document, activeEditKey, editPrefix + "radius", "Radius", shape->radius);
        editIntRange(document, activeEditKey, editPrefix + "segments", "Segments", shape->segments, 3, 256);
    }
    else if (shape->type == "Polygon")
    {
        editPolygonPoints(document, *shape, activeEditKey, editPrefix);
    }
    else
    {
        editSize(document, activeEditKey, editPrefix + "size", "Size", shape->size);
    }

    drawShapePropertiesTable(document, *shape);
    ImGui::EndDisabled();
}
}  // namespace editor
