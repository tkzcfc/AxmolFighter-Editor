#include "combat/CombatBoxProjection.h"

#include <array>

namespace editor
{
namespace
{
ImU32 darken(ImU32 color)
{
    const int r = static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xff);
    const int g = static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xff);
    const int b = static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xff);
    const int a = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xff);
    return IM_COL32(static_cast<int>(r * 0.8f), static_cast<int>(g * 0.8f), static_cast<int>(b * 0.8f), a);
}

void drawFace(ImDrawList& drawList,
              const std::array<ImVec2, 4>& points,
              ImU32 fillColor,
              ImU32 borderColor,
              float borderWidth)
{
    drawList.AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), fillColor);
    drawList.AddPolyline(points.data(), static_cast<int>(points.size()), borderColor, ImDrawFlags_Closed, borderWidth);
}
}  // namespace

ProjectedCombatBox projectCombatBox(const CombatBox& box)
{
    // 斜二测：深度轴 y 投影到右上 45° ×0.5；front/back 分别用 pos.y 与 pos.y+size.y
    constexpr float oblique = 0.5f;
    const float fx = static_cast<float>(box.pos.x);
    const float fz = static_cast<float>(box.pos.z);
    const float sx = static_cast<float>(box.size.x);
    const float sy = static_cast<float>(box.size.y);
    const float sz = static_cast<float>(box.size.z);
    const float y0 = static_cast<float>(box.pos.y);
    const float y1 = y0 + sy;
    const float dox0 = y0 * oblique;
    const float doz0 = y0 * oblique;
    const float dox1 = y1 * oblique;
    const float doz1 = y1 * oblique;

    return {{fx + dox0, fz + doz0},
            {fx + sx + dox0, fz + doz0},
            {fx + sx + dox0, fz + sz + doz0},
            {fx + dox0, fz + sz + doz0},
            {fx + dox1, fz + doz1},
            {fx + sx + dox1, fz + doz1},
            {fx + sx + dox1, fz + sz + doz1},
            {fx + dox1, fz + sz + doz1}};
}

void drawCombatBox(ImDrawList& drawList,
                   const CombatBox& box,
                   const std::function<ImVec2(const ImVec2&)>& worldToScreen,
                   ImU32 borderColor,
                   ImU32 fillColor,
                   bool selected)
{
    const ProjectedCombatBox projected = projectCombatBox(box);
    auto screen = [&worldToScreen](const ImVec2& point) { return worldToScreen(point); };
    const ImVec2 FL = screen(projected.FL);
    const ImVec2 FR = screen(projected.FR);
    const ImVec2 FRT = screen(projected.FRT);
    const ImVec2 FLT = screen(projected.FLT);
    const ImVec2 BL = screen(projected.BL);
    const ImVec2 BR = screen(projected.BR);
    const ImVec2 BRT = screen(projected.BRT);
    const ImVec2 BLT = screen(projected.BLT);

    const ImU32 sideFill = darken(fillColor);
    const float borderWidth = selected ? 2.5f : 1.0f;
    drawFace(drawList, {FLT, FRT, BRT, BLT}, sideFill, borderColor, borderWidth);
    drawFace(drawList, {FR, BR, BRT, FRT}, sideFill, borderColor, borderWidth);
    drawFace(drawList, {FL, FR, FRT, FLT}, fillColor, borderColor, borderWidth);
}
}  // namespace editor
