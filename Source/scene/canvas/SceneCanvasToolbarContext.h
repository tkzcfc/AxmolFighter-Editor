#pragma once

#include <string>
#include <string_view>

namespace editor
{
class SceneCanvasToolbarContext
{
public:
    explicit SceneCanvasToolbarContext(std::string& activeToolId);

    const std::string& activeToolId() const;
    bool isActive(std::string_view toolId) const;
    void activate(std::string_view toolId);
    bool drawToolButton(std::string_view toolId, const char* label);
    bool drawToolButton(std::string_view toolId,
                        const char* label,
                        const char* icon,
                        unsigned int iconGlyph,
                        const char* tooltip,
                        const char* fallbackLabel);

private:
    std::string& m_activeToolId;
};
}  // namespace editor
