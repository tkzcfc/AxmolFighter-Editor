#include "tools/ToolRegistry.h"

namespace editor
{
void ToolRegistry::add(std::unique_ptr<IEditorTool> tool)
{
    m_tools.push_back(std::move(tool));
}

bool ToolRegistry::activate(EditorContext& context, std::string_view id)
{
    for (auto& tool : m_tools)
    {
        if (tool->getId() != id)
            continue;

        if (m_activeTool == tool.get())
            return true;

        if (m_activeTool)
            m_activeTool->onDeactivate(context);

        m_activeTool = tool.get();
        m_activeTool->onActivate(context);
        return true;
    }
    return false;
}

IEditorTool* ToolRegistry::activeTool()
{
    return m_activeTool;
}

const std::vector<std::unique_ptr<IEditorTool>>& ToolRegistry::tools() const
{
    return m_tools;
}

void ToolRegistry::updateActive(EditorContext& context, float deltaTime)
{
    if (m_activeTool)
        m_activeTool->onUpdate(context, deltaTime);
}

void ToolRegistry::renderActive(EditorContext& context)
{
    if (m_activeTool)
        m_activeTool->onImGuiRender(context);
}
}  // namespace editor
