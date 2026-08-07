#pragma once

#include "asset_browser/AssetDragPayload.h"
#include "editor_properties/EditorProperty.h"

#include <functional>
#include <initializer_list>
#include <vector>

namespace editor
{
class AssetReferenceEditorProperty final : public EditorProperty
{
public:
    using AssignCallback = std::function<void(EditorPropertyContext&, const AssetDragPayload&)>;
    using AfterEditCallback = std::function<void(EditorPropertyContext&)>;

    AssetReferenceEditorProperty(std::string id,
                                std::string label,
                                std::string& value,
                                std::initializer_list<AssetKind> allowedKinds,
                                AssignCallback assign,
                                AfterEditCallback afterEdit = {});
    void draw(EditorPropertyContext& context) override;

private:
    std::string& m_value;
    std::vector<AssetKind> m_allowedKinds;
    AssignCallback m_assign;
    AfterEditCallback m_afterEdit;
};
}  // namespace editor
