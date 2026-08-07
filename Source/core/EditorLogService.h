#pragma once

#include "base/Logging.h"
#include "core/IService.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace editor
{
class EditorLogService final : public IService, public ax::ILogOutput
{
public:
    struct DisplayEntry
    {
        std::uint64_t sequence = 0;
        ax::LogLevel level = ax::LogLevel::Info;
        std::string tag;
        std::string text;
        std::size_t count = 1;
    };

    static constexpr std::size_t DefaultMaxEntries = 1000;

    void write(ax::LogItem& item, const char* tag) override;

    std::vector<DisplayEntry> snapshot(bool collapse) const;

    void clear();

    std::uint64_t revision() const;
    std::size_t entryCount() const;

    bool collapseEnabled() const;
    void setCollapseEnabled(bool enabled);

    bool autoScrollEnabled() const;
    void setAutoScrollEnabled(bool enabled);

private:
    struct Entry
    {
        std::uint64_t sequence = 0;
        ax::LogLevel level = ax::LogLevel::Info;
        std::string tag;
        std::string text;
        std::string canonicalText;
    };

    mutable std::mutex m_mutex;
    std::deque<Entry> m_entries;
    std::uint64_t m_nextSequence = 0;
    std::uint64_t m_revision = 0;
    std::size_t m_maxEntries = DefaultMaxEntries;
    bool m_collapseEnabled = true;
    bool m_autoScrollEnabled = true;
};
}  // namespace editor
