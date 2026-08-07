#pragma once

#include "imgui.h"

namespace editor
{
class EditorDockSpace
{
public:
    ImGuiID draw();
    static ImGuiID dockSpaceId();

private:
    ImGuiID drawDockSpace();
};
}  // namespace editor
