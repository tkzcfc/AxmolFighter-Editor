#pragma once

#include "modules/PanelModule.h"

namespace editor
{
class ShapeListModule final : public PanelModule
{
public:
    ShapeListModule();

private:
    void drawContent(EditorContext& context) override;
};
}  // namespace editor
