#pragma once

#include "documents/IEditorDocument.h"

namespace editor
{
class TextDocument final : public IEditorDocument
{
public:
    bool open(const std::filesystem::path& path) override;
    bool save() override;
    bool isDirty() const override;
    std::string getDisplayName() const override;
    const std::filesystem::path& path() const override;
    std::string lastError() const override;

    std::string& content();
    const std::string& content() const;
    void markDirty();

private:
    std::filesystem::path m_path;
    std::string m_content;
    std::string m_lastError;
    bool m_dirty = false;
};
}  // namespace editor
