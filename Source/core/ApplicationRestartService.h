#pragma once

#include "core/IService.h"

#include <string>

namespace editor
{
class ApplicationRestartService final : public IService
{
public:
    bool restart(std::string* message = nullptr) const;
};
}  // namespace editor
