#pragma once

#include "core/IService.h"

namespace editor
{
class LayoutService : public IService
{
public:
    void requestResetLayout();
    bool consumeResetLayoutRequest();

private:
    bool m_resetRequested = false;
};
}  // namespace editor
