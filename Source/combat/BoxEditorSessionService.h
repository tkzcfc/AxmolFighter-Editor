#pragma once

#include "animation/AnimationPreviewController.h"
#include "combat/CombatBoxTypes.h"
#include "core/IService.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace editor
{
class BoxDocument;
class ProjectSettings;

class BoxEditorSessionService final : public IService
{
public:
    void bind(BoxDocument& document, const ProjectSettings& settings);
    void clear();
    void update(float deltaTime);

    BoxDocument* document() const;
    AnimationPreviewController& preview();
    const AnimationPreviewController& preview() const;
    const std::string& previewError() const;

    std::int32_t playheadMs() const;
    void setPlayheadMs(std::int32_t value);
    bool isPlaying() const;
    void setPlaying(bool playing);

    int selectedTrack = -1;
    int selectedKey = -1;
    int selectedEvent = -1;

    bool hasTrackClipboard() const;
    const BoxTrack& trackClipboard() const;
    void setTrackClipboard(const BoxTrack& track);
    void clearTrackClipboard();
    bool hasKeyClipboard() const;
    const BoxKey& keyClipboard() const;
    void setKeyClipboard(const BoxKey& key);
    void clearKeyClipboard();

private:
    BoxDocument* m_document = nullptr;
    std::filesystem::path m_documentPath;
    std::int32_t m_playheadMs = 0;
    float m_playheadRemainder = 0.0f;
    bool m_playing = false;
    std::string m_previewError;
    AnimationPreviewController m_preview;
    bool m_hasTrackClipboard = false;
    bool m_hasKeyClipboard = false;
    BoxTrack m_trackClipboard;
    BoxKey m_keyClipboard;
};
}  // namespace editor
