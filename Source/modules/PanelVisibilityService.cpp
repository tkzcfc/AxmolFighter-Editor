#include "modules/PanelVisibilityService.h"

#include <utility>

namespace editor
{
void PanelVisibilityService::open(std::string panelId)
{
    setOpen(std::move(panelId), true);
}

void PanelVisibilityService::close(std::string panelId)
{
    setOpen(std::move(panelId), false);
}

void PanelVisibilityService::setOpen(std::string panelId, bool open)
{
    m_openPanels[std::move(panelId)] = open;
}

bool PanelVisibilityService::isOpen(const std::string& panelId) const
{
    const auto it = m_openPanels.find(panelId);
    return it != m_openPanels.end() && it->second;
}
}  // namespace editor
