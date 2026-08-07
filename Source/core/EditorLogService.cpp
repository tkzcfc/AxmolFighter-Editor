#include "core/EditorLogService.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace editor
{
namespace
{
std::string_view trimTrailingNewlines(std::string_view value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.remove_suffix(1);
    return value;
}

std::string_view trimLeadingWhitespace(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    return value;
}

bool isTimestampSegment(std::string_view value)
{
    return value.size() >= 19 && value[4] == '-' && value[7] == '-' && value[10] == ' ' && value[13] == ':' &&
           value[16] == ':';
}

std::string canonicalizeMessage(std::string_view text)
{
    text = trimTrailingNewlines(text);
    if (text.size() >= 2 && text[1] == '/')
        text.remove_prefix(2);

    for (;;)
    {
        if (text.empty() || text.front() != '[')
            break;

        const std::size_t closeIndex = text.find(']');
        if (closeIndex == std::string_view::npos)
            break;

        const std::string_view segment = text.substr(1, closeIndex - 1);
        if (!isTimestampSegment(segment) && !segment.starts_with("PID:") && !segment.starts_with("TID:"))
            break;

        text.remove_prefix(closeIndex + 1);
    }

    text = trimLeadingWhitespace(text);
    return std::string(text);
}

std::string makeCollapseKey(ax::LogLevel level, const std::string& tag, const std::string& canonicalText)
{
    std::string key;
    key.reserve(tag.size() + canonicalText.size() + 16);
    key.append(std::to_string(static_cast<int>(level)));
    key.push_back('\x1f');
    key.append(tag);
    key.push_back('\x1f');
    key.append(canonicalText);
    return key;
}
}  // namespace

void EditorLogService::write(ax::LogItem& item, const char* tag)
{
    Entry entry;
    entry.level = item.level();
    entry.tag = tag ? tag : "";
    entry.text = std::string(trimTrailingNewlines(item.message()));
    entry.canonicalText = canonicalizeMessage(entry.text);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        entry.sequence = ++m_nextSequence;
        m_entries.push_back(std::move(entry));
        while (m_entries.size() > m_maxEntries)
            m_entries.pop_front();
        ++m_revision;
    }

    ax::writeLog(item, tag);
}

std::vector<EditorLogService::DisplayEntry> EditorLogService::snapshot(bool collapse) const
{
    std::deque<Entry> entries;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        entries = m_entries;
    }

    std::vector<DisplayEntry> displayEntries;
    displayEntries.reserve(entries.size());

    if (!collapse)
    {
        for (const Entry& entry : entries)
        {
            displayEntries.push_back({entry.sequence, entry.level, entry.tag, entry.text, 1});
        }
        return displayEntries;
    }

    std::unordered_map<std::string, std::size_t> entryIndices;
    entryIndices.reserve(entries.size());

    for (const Entry& entry : entries)
    {
        const std::string key = makeCollapseKey(entry.level, entry.tag, entry.canonicalText);
        const auto it = entryIndices.find(key);
        if (it == entryIndices.end())
        {
            entryIndices.emplace(key, displayEntries.size());
            displayEntries.push_back({entry.sequence, entry.level, entry.tag, entry.text, 1});
            continue;
        }

        DisplayEntry& displayEntry = displayEntries[it->second];
        ++displayEntry.count;
        displayEntry.sequence = entry.sequence;
        displayEntry.text = entry.text;
        displayEntry.tag = entry.tag;
    }

    std::stable_sort(displayEntries.begin(), displayEntries.end(), [](const DisplayEntry& lhs, const DisplayEntry& rhs) {
        return lhs.sequence < rhs.sequence;
    });
    return displayEntries;
}

void EditorLogService::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    ++m_revision;
}

std::uint64_t EditorLogService::revision() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_revision;
}

std::size_t EditorLogService::entryCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

bool EditorLogService::collapseEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_collapseEnabled;
}

void EditorLogService::setCollapseEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_collapseEnabled = enabled;
}

bool EditorLogService::autoScrollEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoScrollEnabled;
}

void EditorLogService::setAutoScrollEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoScrollEnabled = enabled;
}
}  // namespace editor
