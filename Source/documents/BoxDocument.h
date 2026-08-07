#pragma once

#include "combat/CombatBoxTypes.h"
#include "documents/IEditorDocument.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
class BoxDocument final : public IEditorDocument
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

    BoxData& data();
    const BoxData& data() const;
    bool beginEditTransaction();
    void commitEditTransaction();
    void cancelEditTransaction();
    void markDirty();
    void normalizeKeys();
    void normalizeToDuration();
    std::size_t revision() const;

private:
    bool loadFromJson(const std::string& json);
    std::string toJson() const;
    void pushUndoSnapshot(const BoxData& snapshot);

    std::filesystem::path m_path;
    BoxData m_data;
    BoxData m_savedData;
    BoxData m_undoCheckpoint;
    BoxData m_transactionSnapshot;
    std::string m_lastError;
    std::vector<BoxData> m_undoStack;
    std::vector<BoxData> m_redoStack;
    bool m_editTransactionActive = false;
    std::size_t m_revision = 0;
};
}  // namespace editor
