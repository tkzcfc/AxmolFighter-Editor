#pragma once

struct ImFont;

namespace editor
{
inline constexpr const char* kEditorIconEye      = "\xef\x81\xae";
inline constexpr const char* kEditorIconEyeSlash = "\xef\x81\xb0";
inline constexpr const char* kEditorIconLock     = "\xef\x80\xa3";
inline constexpr const char* kEditorIconUnlock   = "\xef\x82\x9c";

ImFont* editorIconFont();
bool drawEditorIconToggle(const char* icon, const char* tooltip, bool& value, bool disabled = false);
}  // namespace editor
