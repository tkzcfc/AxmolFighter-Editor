#include "core/LayoutService.h"

namespace editor
{
void LayoutService::requestResetLayout()
{
    m_resetRequested = true;
}

bool LayoutService::consumeResetLayoutRequest()
{
    const bool requested = m_resetRequested;
    m_resetRequested = false;
    return requested;
}
}  // namespace editor
