#include "core/ProjectSettings.h"

#include "platform/FileUtils.h"

#include <filesystem>

namespace editor
{
namespace
{
std::string normalizePath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string resolveDirectoryPath(const std::string& path)
{
    if (path.empty())
        return {};

    const std::filesystem::path filesystemPath(path);
    if (filesystemPath.is_absolute())
        return normalizePath(filesystemPath);

    ax::FileUtils* fileUtils = ax::FileUtils::getInstance();
    if (fileUtils)
    {
        std::string resolved = fileUtils->fullPathForDirectory(path);
        if (resolved.empty())
            resolved = fileUtils->fullPathForFilename(path);
        if (!resolved.empty())
            return normalizePath(resolved);
    }

    return normalizePath(std::filesystem::absolute(filesystemPath));
}
}  // namespace

ProjectSettings::ProjectSettings()
{
    m_resourceRoot = resolveDirectoryPath("Content");
    m_configRoot = resolveDirectoryPath("Content");
}

const std::string& ProjectSettings::resourceRoot() const
{
    return m_resourceRoot;
}

const std::string& ProjectSettings::configRoot() const
{
    return m_configRoot;
}

void ProjectSettings::setResourceRoot(std::string path)
{
    m_resourceRoot = resolveDirectoryPath(path);
}

void ProjectSettings::setConfigRoot(std::string path)
{
    m_configRoot = resolveDirectoryPath(path);
}

std::filesystem::path ProjectSettings::resolvedResourceRoot() const
{
    return std::filesystem::path(m_resourceRoot).lexically_normal();
}

std::filesystem::path ProjectSettings::resolvedConfigRoot() const
{
    return std::filesystem::path(m_configRoot).lexically_normal();
}
}  // namespace editor
