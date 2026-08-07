#pragma once

#include "tools/IEditorTool.h"

#include <memory>
#include <vector>

namespace editor
{
class EditorContext;

class ToolRegistry
{
public:
    void add(std::unique_ptr<IEditorTool> tool);
    bool activate(EditorContext& context, std::string_view id);
    IEditorTool* activeTool();
    const std::vector<std::unique_ptr<IEditorTool>>& tools() const;
    void updateActive(EditorContext& context, float deltaTime);
    void renderActive(EditorContext& context);

private:
    std::vector<std::unique_ptr<IEditorTool>> m_tools;
    IEditorTool* m_activeTool = nullptr;
};
}  // namespace editor
