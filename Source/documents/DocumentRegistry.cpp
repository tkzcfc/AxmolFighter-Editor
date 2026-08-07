#include "documents/DocumentRegistry.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace editor
{
namespace
{
std::filesystem::path normalizedPath(const std::filesystem::path& path)
{
    if (path.empty())
        return {};

    std::error_code error;
    auto absolutePath = std::filesystem::absolute(path, error);
    if (error)
        absolutePath = path;

    return absolutePath.lexically_normal();
}

std::string comparablePathKey(const std::filesystem::path& path)
{
    std::string key = normalizedPath(path).generic_string();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
#endif
    return key;
}
}  // namespace

IEditorDocument* DocumentRegistry::activeDocument()
{
    return hasActiveDocument() ? m_documents[m_activeIndex].get() : nullptr;
}

const IEditorDocument* DocumentRegistry::activeDocument() const
{
    return hasActiveDocument() ? m_documents[m_activeIndex].get() : nullptr;
}

std::vector<std::unique_ptr<IEditorDocument>>& DocumentRegistry::documents()
{
    return m_documents;
}

const std::vector<std::unique_ptr<IEditorDocument>>& DocumentRegistry::documents() const
{
    return m_documents;
}

std::size_t DocumentRegistry::activeIndex() const
{
    return m_activeIndex;
}

std::size_t DocumentRegistry::activeRevision() const
{
    return m_activeRevision;
}

bool DocumentRegistry::setActiveIndex(std::size_t index)
{
    if (index >= m_documents.size())
        return false;

    if (m_activeIndex == index)
        return true;

    m_activeIndex = index;
    ++m_activeRevision;
    return true;
}

IEditorDocument* DocumentRegistry::findByPath(const std::filesystem::path& path)
{
    const std::size_t index = findIndexByPath(path);
    return index == npos ? nullptr : m_documents[index].get();
}

IEditorDocument* DocumentRegistry::openOrActivate(std::unique_ptr<IEditorDocument> document, bool* activatedExisting)
{
    if (activatedExisting)
        *activatedExisting = false;

    if (!document)
        return activeDocument();

    const std::size_t existingIndex = findIndexByPath(document->path());
    if (existingIndex != npos)
    {
        if (m_activeIndex != existingIndex)
        {
            m_activeIndex = existingIndex;
            ++m_activeRevision;
        }
        if (activatedExisting)
            *activatedExisting = true;
        return m_documents[m_activeIndex].get();
    }

    m_documents.push_back(std::move(document));
    m_activeIndex = m_documents.size() - 1;
    ++m_activeRevision;
    return m_documents[m_activeIndex].get();
}

bool DocumentRegistry::closeDocument(std::size_t index, std::string& message, bool discardDirty)
{
    message.clear();
    if (index >= m_documents.size())
    {
        message = "Document index is invalid.";
        return false;
    }
    if (m_documents[index]->isDirty() && !discardDirty)
    {
        message = "Save or discard changes before closing.";
        m_activeIndex = index;
        return false;
    }

    m_documents.erase(m_documents.begin() + static_cast<std::ptrdiff_t>(index));
    if (m_documents.empty())
    {
        m_activeIndex = npos;
        ++m_activeRevision;
    }
    else if (m_activeIndex == index)
    {
        m_activeIndex = std::min(index, m_documents.size() - 1);
        ++m_activeRevision;
    }
    else if (m_activeIndex > index)
    {
        --m_activeIndex;
        ++m_activeRevision;
    }
    return true;
}

std::size_t DocumentRegistry::closeCleanDocuments(std::size_t& dirtyCount)
{
    dirtyCount = 0;
    std::size_t closedCount = 0;
    for (std::size_t index = m_documents.size(); index > 0; --index)
    {
        const std::size_t documentIndex = index - 1;
        if (m_documents[documentIndex]->isDirty())
        {
            ++dirtyCount;
            continue;
        }

        m_documents.erase(m_documents.begin() + static_cast<std::ptrdiff_t>(documentIndex));
        ++closedCount;
    }

    if (m_documents.empty())
    {
        m_activeIndex = npos;
        ++m_activeRevision;
    }
    else if (m_activeIndex >= m_documents.size())
    {
        m_activeIndex = m_documents.size() - 1;
        ++m_activeRevision;
    }
    return closedCount;
}

bool DocumentRegistry::saveAll(std::string& message, std::size_t& savedCount)
{
    message.clear();
    savedCount = 0;
    for (const auto& document : m_documents)
    {
        if (!document || !document->isDirty())
            continue;

        if (!document->save())
        {
            message = "Save failed: " + document->lastError();
            return false;
        }
        ++savedCount;
    }
    return true;
}

bool DocumentRegistry::hasDirtyDocuments() const
{
    return std::any_of(m_documents.begin(), m_documents.end(), [](const auto& document) {
        return document && document->isDirty();
    });
}

std::size_t DocumentRegistry::findIndexByPath(const std::filesystem::path& path) const
{
    const std::string key = comparablePathKey(path);
    if (key.empty())
        return npos;

    for (std::size_t index = 0; index < m_documents.size(); ++index)
    {
        if (comparablePathKey(m_documents[index]->path()) == key)
            return index;
    }
    return npos;
}

bool DocumentRegistry::hasActiveDocument() const
{
    return m_activeIndex < m_documents.size();
}
}  // namespace editor
