#include "ui/EditorIcons.h"

#include "imgui.h"

namespace editor
{
ImFont* editorIconFont()
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    if (!atlas || atlas->Fonts.Size == 0)
        return nullptr;
    for (ImFont* font : atlas->Fonts)
    {
        if (font && font->IsGlyphInFont(0xf06e) && font->IsGlyphInFont(0xf023))
            return font;
    }
    return nullptr;
}

bool drawEditorIconToggle(const char* icon, const char* tooltip, bool& value, bool disabled)
{
    ImGui::BeginDisabled(disabled);
    ImFont* font = editorIconFont();
    if (font)
        ImGui::PushFont(font, 0.0f);
    const bool clicked = ImGui::SmallButton(icon);
    if (font)
        ImGui::PopFont();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);
    if (clicked)
        value = !value;
    return clicked;
}
}  // namespace editor
