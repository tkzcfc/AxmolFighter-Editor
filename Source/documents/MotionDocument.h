#pragma once

#include "documents/IEditorDocument.h"
#include "motion/MotionTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
class MotionDocument final : public IEditorDocument
{
public:
    bool open(const std::filesystem::path& path) override;
    bool save() override;
    bool isDirty() const override;
    std::string getDisplayName() const override;
    const std::filesystem::path& path() const override;
    std::string lastError() const override;
    bool canUndo() const override;
    bool undo() override;
    bool canRedo() const override;
    bool redo() override;

    static bool createDefaultFile(const std::filesystem::path& path, std::string& message);

    MotionData& data();
    const MotionData& data() const;
    bool beginEditTransaction();
    void commitEditTransaction();
    void cancelEditTransaction();
    void markDirty();
    std::size_t revision() const;

private:
    bool loadFromJson(const std::string& json);
    std::string toJson() const;
    void pushUndoSnapshot(const MotionData& snapshot);
    void sortMotionsByName();

    std::filesystem::path m_path;
    MotionData m_data;
    MotionData m_savedData;
    MotionData m_undoCheckpoint;
    MotionData m_transactionSnapshot;
    std::string m_lastError;
    std::vector<MotionData> m_undoStack;
    std::vector<MotionData> m_redoStack;
    bool m_editTransactionActive = false;
    std::size_t m_revision = 0;
};
}  // namespace editor
