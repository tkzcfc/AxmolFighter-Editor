#pragma once

#include "motion/MotionTypes.h"

#include <cstddef>
#include <string>

namespace editor
{
class EditorContext;
class MotionDocument;

class MotionEditor
{
public:
    void drawContent(EditorContext& context, MotionDocument& document);
    void drawInspector(EditorContext& context, MotionDocument& document);
    void reset();

private:
    void drawMotionList(MotionDocument& document);
    void drawAnimationTable(EditorContext& context, MotionDocument& document);
    void syncSelection(MotionDocument& document);
    bool acceptAniDrop(MotionDocument& document, MotionAnimationEntry& entry);
    bool acceptBoxDrop(MotionDocument& document, std::string& boxField);

    std::string m_documentPath;
    std::string m_motionFilter;
    std::string m_selectedMotionName;
    int m_selectedAnimation = -1;
    std::size_t m_boundRevision = 0;
};
}  // namespace editor
