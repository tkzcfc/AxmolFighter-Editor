#include "documents/TextDocument.h"

#include <fstream>
#include <sstream>

namespace editor
{
bool TextDocument::open(const std::filesystem::path& path)
{
    m_lastError.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_lastError = "Failed to open text file: " + path.generic_string();
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    m_content = buffer.str();
    m_path = path.lexically_normal();
    m_dirty = false;
    return true;
}

bool TextDocument::save()
{
    m_lastError.clear();
    if (m_path.empty())
    {
        m_lastError = "Text document path is empty.";
        return false;
    }

    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        m_lastError = "Failed to open text file for writing: " + m_path.generic_string();
        return false;
    }

    file << m_content;
    if (!file)
    {
        m_lastError = "Failed to write text file: " + m_path.generic_string();
        return false;
    }

    m_dirty = false;
    return true;
}

bool TextDocument::isDirty() const
{
    return m_dirty;
}

std::string TextDocument::getDisplayName() const
{
    return m_path.empty() ? "Untitled" : m_path.filename().generic_string();
}

const std::filesystem::path& TextDocument::path() const
{
    return m_path;
}

std::string TextDocument::lastError() const
{
    return m_lastError;
}

std::string& TextDocument::content()
{
    return m_content;
}

const std::string& TextDocument::content() const
{
    return m_content;
}

void TextDocument::markDirty()
{
    m_dirty = true;
}
}  // namespace editor
