#pragma once

#include "scene/SceneDocument.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace editor
{
class EditorContext;

struct EditorPropertyContext
{
    EditorContext* editorContext = nullptr;
    SceneDocument& document;
    SceneNode& node;
    std::string& activeEditKey;
};

class EditorProperty
{
public:
    EditorProperty(std::string id, std::string label, bool disableWhenNodeLocked = true);
    virtual ~EditorProperty() = default;

    const std::string& id() const;
    const std::string& label() const;
    void drawWithState(EditorPropertyContext& context);
    virtual void draw(EditorPropertyContext& context) = 0;

protected:
    bool beginEdit(EditorPropertyContext& context) const;
    void finishEdit(EditorPropertyContext& context, bool prepared, bool changed) const;

private:
    std::string m_id;
    std::string m_label;
    bool m_disableWhenNodeLocked = true;
};

struct EditorPropertyGroup
{
    std::string label;
    std::vector<std::unique_ptr<EditorProperty>> properties;
};

void drawEditorPropertyGroups(EditorPropertyContext& context, const std::vector<EditorPropertyGroup>& groups);
}  // namespace editor
