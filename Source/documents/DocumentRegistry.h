#pragma once

#include "documents/IEditorDocument.h"

#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace editor
{
class DocumentRegistry
{
public:
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    IEditorDocument* activeDocument();
    const IEditorDocument* activeDocument() const;
    std::vector<std::unique_ptr<IEditorDocument>>& documents();
    const std::vector<std::unique_ptr<IEditorDocument>>& documents() const;
    std::size_t activeIndex() const;
    std::size_t activeRevision() const;
    bool setActiveIndex(std::size_t index);
    IEditorDocument* findByPath(const std::filesystem::path& path);
    IEditorDocument* openOrActivate(std::unique_ptr<IEditorDocument> document, bool* activatedExisting = nullptr);
    bool closeDocument(std::size_t index, std::string& message, bool discardDirty = false);
    std::size_t closeCleanDocuments(std::size_t& dirtyCount);
    bool saveAll(std::string& message, std::size_t& savedCount);
    bool hasDirtyDocuments() const;

private:
    std::size_t findIndexByPath(const std::filesystem::path& path) const;
    bool hasActiveDocument() const;

    std::vector<std::unique_ptr<IEditorDocument>> m_documents;
    std::size_t m_activeIndex = npos;
    std::size_t m_activeRevision = 0;
};
}  // namespace editor
