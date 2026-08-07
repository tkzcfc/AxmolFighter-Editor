#include "scene/SceneNodeClipboardService.h"

#include <utility>

namespace editor
{
namespace
{
constexpr SceneVec2 kSameDocumentPasteOffset{16.0f, -16.0f};
}

bool SceneNodeClipboardService::copyNode(const SceneDocument& document,
                                         const std::filesystem::path& documentKey,
                                         const std::string& nodeId)
{
    SceneNodeCopyData copy;
    if (!document.copyNode(nodeId, copy))
    {
        clear();
        return false;
    }

    m_clipboard = Clipboard{std::move(copy), documentKey.lexically_normal()};
    return true;
}

bool SceneNodeClipboardService::copySelectedNode(const SceneDocument& document,
                                                 const std::filesystem::path& documentKey)
{
    return copyNode(document, documentKey, document.selectedNodeId());
}

bool SceneNodeClipboardService::pasteNode(SceneDocument& document, const std::filesystem::path& documentKey)
{
    if (!m_clipboard)
        return false;

    const bool sameDocument = m_clipboard->sourceDocumentKey == documentKey.lexically_normal();
    const std::string preferredParentId = sameDocument ? m_clipboard->copy.parentNodeId : "root";
    return pasteNode(document, documentKey, preferredParentId);
}

bool SceneNodeClipboardService::pasteNode(SceneDocument& document,
                                          const std::filesystem::path& documentKey,
                                          const std::string& parentNodeId)
{
    if (!m_clipboard)
        return false;

    const bool sameDocument = m_clipboard->sourceDocumentKey == documentKey.lexically_normal();
    const SceneVec2 offset = sameDocument ? kSameDocumentPasteOffset : SceneVec2{};
    return document.pasteNodeCopy(m_clipboard->copy, parentNodeId, offset) != nullptr;
}

void SceneNodeClipboardService::clear()
{
    m_clipboard.reset();
}

bool SceneNodeClipboardService::hasNode() const
{
    return m_clipboard.has_value();
}
}  // namespace editor
