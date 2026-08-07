#pragma once

#include "core/EditorContext.h"

namespace editor
{
class EditorRuntime
{
public:
    void initialize();
    void shutdown();
    void update(float deltaTime);
    void renderImGui();

    EditorContext& context();

private:
    void registerDefaultModules();

    EditorContext m_context;
    bool m_modulesRegistered = false;
    bool m_initialized = false;
};
}  // namespace editor
