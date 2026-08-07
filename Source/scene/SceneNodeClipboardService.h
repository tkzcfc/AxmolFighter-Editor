#pragma once

#include "core/IService.h"
#include "scene/SceneDocument.h"

#include <filesystem>
#include <optional>

namespace editor
{
class SceneNodeClipboardService final : public IService
{
public:
    bool copyNode(const SceneDocument& document,
                  const std::filesystem::path& documentKey,
                  const std::string& nodeId);
    bool copySelectedNode(const SceneDocument& document, const std::filesystem::path& documentKey);
    bool pasteNode(SceneDocument& document, const std::filesystem::path& documentKey);
    bool pasteNode(SceneDocument& document,
                   const std::filesystem::path& documentKey,
                   const std::string& parentNodeId);
    void clear();
    bool hasNode() const;

private:
    struct Clipboard
    {
        SceneNodeCopyData copy;
        std::filesystem::path sourceDocumentKey;
    };

    std::optional<Clipboard> m_clipboard;
};
}  // namespace editor
