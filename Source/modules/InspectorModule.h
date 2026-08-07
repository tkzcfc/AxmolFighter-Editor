#pragma once

#include "modules/PanelModule.h"

#include <string>

namespace editor
{
class InspectorModule : public PanelModule
{
public:
    InspectorModule();

private:
    void drawContent(EditorContext& context) override;

    std::string m_activeSceneEditKey;
};
}  // namespace editor
