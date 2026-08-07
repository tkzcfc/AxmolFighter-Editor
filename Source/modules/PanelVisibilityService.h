#pragma once

#include "core/IService.h"

#include <map>
#include <string>

namespace editor
{
class PanelVisibilityService final : public IService
{
public:
    void open(std::string panelId);
    void close(std::string panelId);
    void setOpen(std::string panelId, bool open);
    bool isOpen(const std::string& panelId) const;

private:
    std::map<std::string, bool> m_openPanels;
};
}  // namespace editor
