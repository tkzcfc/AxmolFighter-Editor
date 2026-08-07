#include "combat/BoxEditorSessionService.h"

#include "documents/BoxDocument.h"
#include "core/ProjectSettings.h"

#include <algorithm>

namespace editor
{
void BoxEditorSessionService::bind(BoxDocument& document, const ProjectSettings& settings)
{
    if (m_documentPath != document.path())
    {
        m_document = &document;
        m_documentPath = document.path();
        selectedTrack = -1;
        selectedKey = -1;
        selectedEvent = -1;
        m_playheadMs = 0;
        m_playheadRemainder = 0.0f;
        m_playing = false;
    }
    else
    {
        m_document = &document;
    }

    m_preview.load(document, settings);
    m_previewError = m_preview.error();
    if (m_playheadMs >= document.data().duration)
        m_playheadMs = std::max<std::int32_t>(0, document.data().duration - 1);
    m_preview.setTime(m_playheadMs);

    if (selectedTrack < 0 || selectedTrack >= static_cast<int>(document.data().tracks.size()))
    {
        selectedTrack = -1;
        selectedKey = -1;
    }
    else if (selectedKey >= static_cast<int>(document.data().tracks[selectedTrack].keys.size()))
    {
        selectedKey = -1;
    }
    if (selectedEvent >= static_cast<int>(document.data().events.size()))
        selectedEvent = -1;
}

void BoxEditorSessionService::clear()
{
    m_document = nullptr;
    m_documentPath.clear();
    m_preview.reset();
    m_previewError.clear();
    selectedTrack = -1;
    selectedKey = -1;
    selectedEvent = -1;
    m_playheadMs = 0;
    m_playheadRemainder = 0.0f;
    m_playing = false;
}

void BoxEditorSessionService::update(float deltaTime)
{
    if (!m_document || !m_playing)
        return;
    m_playheadRemainder += std::max(0.0f, deltaTime) * 1000.0f;
    const std::int32_t step = static_cast<std::int32_t>(m_playheadRemainder);
    if (step <= 0)
        return;
    m_playheadRemainder -= static_cast<float>(step);
    const std::int32_t duration = m_document->data().duration;
    if (duration <= 0)
    {
        m_playheadMs = 0;
        m_playing = false;
        return;
    }
    m_playheadMs = (m_playheadMs + step) % duration;
    m_preview.setTime(m_playheadMs);
}

BoxDocument* BoxEditorSessionService::document() const { return m_document; }
AnimationPreviewController& BoxEditorSessionService::preview() { return m_preview; }
const AnimationPreviewController& BoxEditorSessionService::preview() const { return m_preview; }
const std::string& BoxEditorSessionService::previewError() const { return m_previewError; }
std::int32_t BoxEditorSessionService::playheadMs() const { return m_playheadMs; }

void BoxEditorSessionService::setPlayheadMs(std::int32_t value)
{
    if (!m_document)
        return;
    const std::int32_t duration = m_document->data().duration;
    m_playheadMs = std::clamp(value, 0, std::max<std::int32_t>(0, duration - 1));
    m_playheadRemainder = 0.0f;
    m_preview.setTime(m_playheadMs);
}

bool BoxEditorSessionService::isPlaying() const { return m_playing; }

void BoxEditorSessionService::setPlaying(bool playing)
{
    m_playing = playing;
}

bool BoxEditorSessionService::hasTrackClipboard() const { return m_hasTrackClipboard; }
const BoxTrack& BoxEditorSessionService::trackClipboard() const { return m_trackClipboard; }
void BoxEditorSessionService::setTrackClipboard(const BoxTrack& track)
{
    m_trackClipboard = track;
    m_hasTrackClipboard = true;
}
void BoxEditorSessionService::clearTrackClipboard() { m_hasTrackClipboard = false; }
bool BoxEditorSessionService::hasKeyClipboard() const { return m_hasKeyClipboard; }
const BoxKey& BoxEditorSessionService::keyClipboard() const { return m_keyClipboard; }
void BoxEditorSessionService::setKeyClipboard(const BoxKey& key)
{
    m_keyClipboard = key;
    m_hasKeyClipboard = true;
}
void BoxEditorSessionService::clearKeyClipboard() { m_hasKeyClipboard = false; }
}  // namespace editor
