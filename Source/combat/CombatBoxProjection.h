#pragma once

#include "combat/CombatBoxTypes.h"
#include "imgui.h"

#include <array>
#include <functional>

namespace editor
{
struct ProjectedCombatBox
{
    ImVec2 FL;
    ImVec2 FR;
    ImVec2 FRT;
    ImVec2 FLT;
    ImVec2 BL;
    ImVec2 BR;
    ImVec2 BRT;
    ImVec2 BLT;
};

ProjectedCombatBox projectCombatBox(const CombatBox& box);

void drawCombatBox(ImDrawList& drawList,
                   const CombatBox& box,
                   const std::function<ImVec2(const ImVec2&)>& worldToScreen,
                   ImU32 borderColor,
                   ImU32 fillColor,
                   bool selected = false);
}  // namespace editor
