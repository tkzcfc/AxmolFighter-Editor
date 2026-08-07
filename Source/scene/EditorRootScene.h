#pragma once

#include "app/EditorRuntime.h"
#include "axmol.h"

namespace editor
{
class EditorRootScene : public ax::Scene
{
public:
    bool init() override;
    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;

private:
    void drawImGui();
    void configureImGui();

    EditorRuntime m_runtime;
    bool m_imguiConfigured = false;
};
}  // namespace editor
