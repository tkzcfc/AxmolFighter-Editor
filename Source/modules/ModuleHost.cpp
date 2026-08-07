#include "modules/ModuleHost.h"

namespace editor
{
void ModuleHost::add(std::unique_ptr<IEditorModule> module)
{
    m_modules.push_back(std::move(module));
}

void ModuleHost::attachAll(EditorContext& context)
{
    if (m_attached)
        return;

    for (auto& module : m_modules)
        module->onAttach(context);

    m_attached = true;
}

void ModuleHost::detachAll(EditorContext& context)
{
    if (!m_attached)
        return;

    for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it)
        (*it)->onDetach(context);

    m_attached = false;
}

void ModuleHost::updateAll(EditorContext& context, float deltaTime)
{
    for (auto& module : m_modules)
        module->onUpdate(context, deltaTime);
}

void ModuleHost::renderAll(EditorContext& context)
{
    for (auto& module : m_modules)
        module->onImGuiRender(context);
}
}  // namespace editor
