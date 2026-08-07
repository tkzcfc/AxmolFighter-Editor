#include "scene/canvas/SceneCanvasToolbarContext.h"

#include "imgui.h"

#include <algorithm>
#include <string>

namespace editor
{
namespace
{
ImFont* iconFontForGlyph(unsigned int glyph)
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    if (!atlas || atlas->Fonts.Size == 0)
        return nullptr;

    for (ImFont* font : atlas->Fonts)
    {
        if (font && font->IsGlyphInFont(static_cast<ImWchar>(glyph)) &&
            (font->IsGlyphInFont(0xf07b) || font->IsGlyphInFont(0xf06e)))
            return font;
    }
    return nullptr;
}

void drawTooltip(const char* label, const char* tooltip)
{
    if (!ImGui::IsItemHovered())
        return;

    if (tooltip && tooltip[0] != '\0')
        ImGui::SetTooltip("%s\n%s", label, tooltip);
    else
        ImGui::SetTooltip("%s", label);
}

void centerNextButton(float width)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    if (availableWidth > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - width) * 0.5f);
}
}  // namespace

SceneCanvasToolbarContext::SceneCanvasToolbarContext(std::string& activeToolId) : m_activeToolId(activeToolId) {}

const std::string& SceneCanvasToolbarContext::activeToolId() const
{
    return m_activeToolId;
}

bool SceneCanvasToolbarContext::isActive(std::string_view toolId) const
{
    return m_activeToolId == toolId;
}

void SceneCanvasToolbarContext::activate(std::string_view toolId)
{
    m_activeToolId = std::string(toolId);
}

bool SceneCanvasToolbarContext::drawToolButton(std::string_view toolId, const char* label)
{
    const bool active = isActive(toolId);
    const ImVec2 size = ImGui::CalcTextSize(label);
    const float buttonWidth = size.x + ImGui::GetStyle().FramePadding.x * 2.0f;
    centerNextButton(buttonWidth);
    if (active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    const bool clicked = ImGui::SmallButton(label);
    if (clicked)
        activate(toolId);

    if (active)
        ImGui::PopStyleColor();

    return clicked;
}

bool SceneCanvasToolbarContext::drawToolButton(std::string_view toolId,
                                               const char* label,
                                               const char* icon,
                                               unsigned int iconGlyph,
                                               const char* tooltip,
                                               const char* fallbackLabel)
{
    const bool active = isActive(toolId);
    const float buttonSize = std::max(28.0f, ImGui::GetFrameHeight());
    const ImVec2 size(buttonSize, buttonSize);
    const std::string id(toolId);

    ImGui::PushID(id.c_str());
    centerNextButton(buttonSize);
    if (active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    ImFont* iconFont = iconGlyph == 0 ? nullptr : iconFontForGlyph(iconGlyph);
    const char* visibleLabel = (iconFont && icon && icon[0] != '\0') ? icon : fallbackLabel;

    if (iconFont)
        ImGui::PushFont(iconFont, 0.0f);
    const bool clicked = ImGui::Button(visibleLabel && visibleLabel[0] != '\0' ? visibleLabel : label, size);
    if (iconFont)
        ImGui::PopFont();

    if (active)
        ImGui::PopStyleColor();
    ImGui::PopID();

    if (clicked)
        activate(toolId);

    drawTooltip(label, tooltip);
    return clicked;
}
}  // namespace editor
