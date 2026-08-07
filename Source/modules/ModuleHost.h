#pragma once

#include "modules/IEditorModule.h"

#include <memory>
#include <vector>

namespace editor
{
class EditorContext;

class ModuleHost
{
public:
    void add(std::unique_ptr<IEditorModule> module);
    void attachAll(EditorContext& context);
    void detachAll(EditorContext& context);
    void updateAll(EditorContext& context, float deltaTime);
    void renderAll(EditorContext& context);

private:
    std::vector<std::unique_ptr<IEditorModule>> m_modules;
    bool m_attached = false;
};
}  // namespace editor
