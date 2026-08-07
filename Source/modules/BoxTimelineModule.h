#pragma once

#include "combat/CombatBoxTypes.h"

#include <cstdint>

namespace editor
{
class BoxDocument;
class EditorContext;

class BoxTimelineEditor final
{
public:
    void draw(EditorContext& context, BoxDocument& document);

private:
    enum class DragMode
    {
        None,
        Key
    };

    std::int32_t snapTime(EditorContext& context, std::int32_t value) const;
    void finishDrag();

    DragMode m_dragMode = DragMode::None;
    int m_dragTrack = -1;
    int m_dragKey = -1;
    std::int32_t m_dragStartMouseTime = 0;
    BoxKey m_dragOriginalKey;
    int m_contextTrack = -1;
    int m_contextKey = -1;
};
}  // namespace editor
