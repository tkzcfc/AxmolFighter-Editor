#pragma once

#include "animation/AnimationTypes.h"
#include "documents/IEditorDocument.h"

#include <string>
#include <vector>

namespace editor
{
class AniDocument final : public IEditorDocument
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

    FrameAnimationData& animation();
    const FrameAnimationData& animation() const;
    bool resolveTemplatePath(const std::string& templatePath, std::string& resolvedPath, std::string& error) const;
    bool resolveTemplatePath(const std::string& templatePath,
                             const AnimationPreviewVariables& variables,
                             std::string& resolvedPath,
                             std::string& error) const;
    bool duplicateFrame(std::size_t frameIndex);
    bool beginEditTransaction();
    void commitEditTransaction();
    void cancelEditTransaction();
    void markDirty();
    std::size_t revision() const;

private:
    bool loadFromJson(const std::string& json);
    std::string toJson() const;
    void pushUndoSnapshot(const FrameAnimationData& snapshot);

    std::filesystem::path m_path;
    FrameAnimationData m_animation;
    std::string m_lastError;
    FrameAnimationData m_savedAnimation;
    FrameAnimationData m_undoCheckpoint;
    std::vector<FrameAnimationData> m_undoStack;
    std::vector<FrameAnimationData> m_redoStack;
    FrameAnimationData m_transactionSnapshot;
    bool m_editTransactionActive = false;
    std::size_t m_revision       = 0;
};
}  // namespace editor
