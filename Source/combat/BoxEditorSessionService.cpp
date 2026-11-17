#include "combat/BoxEditorSessionService.h"

#include "documents/BoxDocument.h"
#include "core/ProjectSettings.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace editor
{
namespace
{
struct FrameSpan
{
    std::int32_t start = 0;
    std::int32_t end = 0;
};

std::vector<FrameSpan> animationFrameSpans(const BoxDocument& document, const AnimationPreviewController& preview)
{
    std::vector<FrameSpan> spans;
    const std::int32_t duration = document.data().duration;
    if (duration <= 0 || !preview.isAni() || !preview.isLoaded() || preview.frameBoundaries().size() < 2)
        return spans;

    const std::vector<std::int32_t>& boundaries = preview.frameBoundaries();
    for (std::size_t index = 0; index + 1 < boundaries.size(); ++index)
    {
        const std::int32_t start = boundaries[index];
        if (start >= duration)
            break;
        const std::int32_t end = std::min(boundaries[index + 1], duration);
        if (end > start)
            spans.push_back({start, end});
    }
    return spans;
}

int frameSpanIndex(const std::vector<FrameSpan>& spans, std::int32_t time)
{
    int index = 0;
    for (int candidate = 0; candidate < static_cast<int>(spans.size()); ++candidate)
    {
        if (spans[candidate].start <= time)
            index = candidate;
        else
            break;
    }
    return index;
}
}  // namespace

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
    const std::int32_t duration = m_document->data().duration;
    if (duration <= 0)
    {
        m_playheadMs = 0;
        m_playheadRemainder = 0.0f;
        m_playing = false;
        m_preview.setTime(m_playheadMs);
        return;
    }

    // 一帧刷新若跨过时间轴的四分之一，60Hz 下会在两个位置间交替，看起来像重影，截图却只有一帧。
    float stepMs = std::max(0.0f, deltaTime) * 1000.0f;
    const float quarter = static_cast<float>(duration) * 0.25f;
    if (stepMs > quarter)
        stepMs = std::max(1.0f, static_cast<float>(duration) / 8.0f);
    m_playheadRemainder += stepMs;
    double cursor = static_cast<double>(m_playheadMs) + static_cast<double>(m_playheadRemainder);
    if (cursor >= static_cast<double>(duration))
        cursor = std::fmod(cursor, static_cast<double>(duration));
    if (cursor < 0.0)
        cursor = 0.0;
    m_playheadMs = static_cast<std::int32_t>(cursor);
    m_playheadRemainder = static_cast<float>(cursor - static_cast<double>(m_playheadMs));
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
    m_playing = false;
    m_playheadMs = std::clamp(value, 0, std::max<std::int32_t>(0, duration - 1));
    m_playheadRemainder = 0.0f;
    m_preview.setTime(m_playheadMs);
}

bool BoxEditorSessionService::isPlaying() const { return m_playing; }

void BoxEditorSessionService::setPlaying(bool playing)
{
    if (!m_document || m_document->data().duration <= 0)
        playing = false;
    m_playing = playing;
    m_playheadRemainder = 0.0f;
}

void BoxEditorSessionService::stepFrame(int direction)
{
    if (!m_document || direction == 0)
        return;
    const std::int32_t duration = m_document->data().duration;
    if (duration <= 0)
        return;

    const std::vector<FrameSpan> spans = animationFrameSpans(*m_document, m_preview);
    if (spans.size() > 1)
    {
        const int index = frameSpanIndex(spans, m_playheadMs);
        int next = index;
        if (direction > 0)
            next = (index + 1) % static_cast<int>(spans.size());
        else if (m_playheadMs <= spans[index].start)
            next = (index - 1 + static_cast<int>(spans.size())) % static_cast<int>(spans.size());
        setPlayheadMs(spans[next].start);
        return;
    }

    std::int32_t target = m_playheadMs + (direction > 0 ? 1 : -1);
    if (target < 0)
        target = duration - 1;
    else if (target >= duration)
        target = 0;
    setPlayheadMs(target);
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
