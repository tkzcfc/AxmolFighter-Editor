#pragma once

#include <filesystem>
#include <string>

namespace editor
{
bool deletePathWithPlatformTrash(const std::filesystem::path& path, std::string& message);
}  // namespace editor
