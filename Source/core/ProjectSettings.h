#pragma once

#include <filesystem>
#include <string>

namespace editor
{
class ProjectSettings
{
public:
    ProjectSettings();

    const std::string& resourceRoot() const;
    const std::string& configRoot() const;
    void setResourceRoot(std::string path);
    void setConfigRoot(std::string path);

    std::filesystem::path resolvedResourceRoot() const;
    std::filesystem::path resolvedConfigRoot() const;

private:
    std::string m_resourceRoot;
    std::string m_configRoot;
};
}  // namespace editor
