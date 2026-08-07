#include "scene/node_editors/SceneNodeEditorUtils.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <cfloat>

namespace editor
{
std::string jsonStringOr(const rapidjson::Value& object, const char* key, const std::string& fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsString())
        return fallback;
    return object[key].GetString();
}

float jsonNumberOr(const rapidjson::Value& object, const char* key, float fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsNumber())
        return fallback;
    return object[key].GetFloat();
}

bool jsonBoolOr(const rapidjson::Value& object, const char* key, bool fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsBool())
        return fallback;
    return object[key].GetBool();
}

SceneColor jsonColorOr(const rapidjson::Value& object, const char* key, SceneColor fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
        return fallback;

    const rapidjson::Value& value = object[key];
    fallback.r = jsonNumberOr(value, "r", fallback.r);
    fallback.g = jsonNumberOr(value, "g", fallback.g);
    fallback.b = jsonNumberOr(value, "b", fallback.b);
    fallback.a = jsonNumberOr(value, "a", fallback.a);
    return fallback;
}

namespace
{
void preparePropertyEdit(SceneDocument& document, std::string& activeEditKey, bool& prepared)
{
    prepared = false;
    if (!activeEditKey.empty() && !document.hasActiveUndoTransaction())
        activeEditKey.clear();
    if (activeEditKey.empty())
        prepared = document.beginUndoTransaction();
}

void finishPropertyEdit(SceneDocument& document,
                        std::string& activeEditKey,
                        const std::string& key,
                        bool prepared,
                        bool changed)
{
    if (changed)
        document.markDirty();

    if ((ImGui::IsItemActivated() || changed) && activeEditKey.empty())
        activeEditKey = key;

    const bool ownsEdit = activeEditKey == key;
    if (ownsEdit && (ImGui::IsItemDeactivated() || (changed && !ImGui::IsItemActive())))
    {
        document.commitUndoTransaction();
        activeEditKey.clear();
        return;
    }

    if (prepared && activeEditKey.empty())
        document.cancelUndoTransaction();
}

bool acceptAssetDrop(SceneDocument& document,
                     std::initializer_list<AssetKind> allowedKinds,
                     const std::function<void(const AssetDragPayload&)>& assign)
{
    if (!ImGui::BeginDragDropTarget())
        return false;

    bool changed = false;
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayloadType))
    {
        if (payload->DataSize == sizeof(AssetDragPayload))
        {
            const auto* asset = static_cast<const AssetDragPayload*>(payload->Data);
            if (std::find(allowedKinds.begin(), allowedKinds.end(), asset->kind) != allowedKinds.end())
            {
                document.beginUndoTransaction();
                assign(*asset);
                document.markDirty();
                document.commitUndoTransaction();
                changed = true;
            }
        }
    }
    ImGui::EndDragDropTarget();
    return changed;
}
}  // namespace

bool editVec2(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, SceneVec2& value)
{
    float data[2] = {value.x, value.y};
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::DragFloat2(label, data);
    if (changed)
    {
        value.x = data[0];
        value.y = data[1];
    }
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editSize(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, SceneSize& value)
{
    float data[2] = {value.width, value.height};
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::DragFloat2(label, data);
    if (changed)
    {
        value.width = std::max(0.0f, data[0]);
        value.height = std::max(0.0f, data[1]);
    }
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editSizeWithPolicy(SceneDocument& document,
                        std::string& activeEditKey,
                        const std::string& key,
                        const char* label,
                        SceneSize& value,
                        const SceneNodeSizeEditPolicy& policy)
{
    ImGui::TextUnformatted(label);
    ImGui::PushID(key.c_str());

    auto editAxis = [&](const char* axisLabel, const std::string& axisKey, float& axisValue, bool editable) {
        bool changed = false;
        if (!editable)
            ImGui::BeginDisabled();

        if (editable)
        {
            bool prepared = false;
            preparePropertyEdit(document, activeEditKey, prepared);
            changed = ImGui::DragFloat(axisLabel, &axisValue);
            if (changed)
                axisValue = std::max(0.0f, axisValue);
            finishPropertyEdit(document, activeEditKey, axisKey, prepared, changed);
        }
        else
        {
            float displayValue = axisValue;
            ImGui::InputFloat(axisLabel, &displayValue, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
        }

        if (!editable)
        {
            ImGui::EndDisabled();
            if (!policy.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", policy.reason.c_str());
        }
        return changed;
    };

    const bool widthChanged = editAxis("Width", key + ".width", value.width, policy.widthEditable);
    const bool heightChanged = editAxis("Height", key + ".height", value.height, policy.heightEditable);
    ImGui::PopID();
    return widthChanged || heightChanged;
}

bool editFloat(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, float& value)
{
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::InputFloat(label, &value);
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editIntRange(SceneDocument& document,
                  std::string& activeEditKey,
                  const std::string& key,
                  const char* label,
                  int& value,
                  int minValue,
                  int maxValue)
{
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::DragInt(label, &value, 1.0f, minValue, maxValue);
    if (changed)
        value = std::clamp(value, minValue, maxValue);
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editBool(SceneDocument& document, const char* label, bool& value)
{
    bool transaction = document.beginUndoTransaction();
    const bool changed = ImGui::Checkbox(label, &value);
    if (changed)
    {
        document.markDirty();
        if (transaction)
            document.commitUndoTransaction();
    }
    else if (transaction)
    {
        document.commitUndoTransaction();
    }
    return changed;
}

bool editString(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, std::string& value)
{
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::InputText(label, &value);
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editMultilineString(SceneDocument& document,
                         std::string& activeEditKey,
                         const std::string& key,
                         const char* label,
                         std::string& value)
{
    ImGui::TextUnformatted(label);
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::InputTextMultiline(("##" + key).c_str(), &value, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f));
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editColor(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, SceneColor& value)
{
    float data[4] = {value.r, value.g, value.b, value.a};
    bool prepared = false;
    preparePropertyEdit(document, activeEditKey, prepared);
    const bool changed = ImGui::ColorEdit4(label, data);
    if (changed)
    {
        value.r = data[0];
        value.g = data[1];
        value.b = data[2];
        value.a = data[3];
    }
    finishPropertyEdit(document, activeEditKey, key, prepared, changed);
    return changed;
}

bool editStringCombo(SceneDocument& document,
                     const char* label,
                     std::string& value,
                     const char* const* items,
                     int itemCount)
{
    int selected = 0;
    for (int index = 0; index < itemCount; ++index)
    {
        if (value == items[index])
        {
            selected = index;
            break;
        }
    }

    if (!ImGui::Combo(label, &selected, items, itemCount))
        return false;

    document.beginUndoTransaction();
    value = items[selected];
    document.markDirty();
    document.commitUndoTransaction();
    return true;
}

bool drawAssetReference(SceneDocument& document,
                        std::string& activeEditKey,
                        const std::string& key,
                        const char* label,
                        std::string& target,
                        std::initializer_list<AssetKind> allowedKinds,
                        const std::function<void(const AssetDragPayload&)>& assign)
{
    const bool changed = editString(document, activeEditKey, key, label, target);
    acceptAssetDrop(document, allowedKinds, assign);
    return changed;
}
}  // namespace editor
