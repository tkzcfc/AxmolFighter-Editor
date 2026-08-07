#pragma once

#include <filesystem>
#include <string>

namespace editor
{
class IEditorDocument
{
public:
    virtual ~IEditorDocument() = default;

    virtual bool open(const std::filesystem::path& path) = 0;
    virtual bool save()                                  = 0;
    virtual bool isDirty() const                         = 0;
    virtual std::string getDisplayName() const           = 0;
    virtual const std::filesystem::path& path() const    = 0;
    virtual std::string lastError() const                = 0;
    virtual bool canUndo() const { return false; }
    virtual bool undo() { return false; }
    virtual bool canRedo() const { return false; }
    virtual bool redo() { return false; }
};
}  // namespace editor
