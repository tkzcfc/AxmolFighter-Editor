#pragma once

#include "rapidjson/document.h"

namespace editor
{
// Top-level key of the opaque generator metadata in editor JSON documents (.box, .motion, .ani, .layer).
inline constexpr const char* kDocumentGenMetaKey = "_gen_meta_";

// Parse flags for editor JSON documents; full precision keeps doubles in the generator metadata exact.
inline constexpr unsigned kDocumentJsonParseFlags = rapidjson::kParseFullPrecisionFlag;

// Opaque metadata written by external generation tools. The editor never shows, edits, validates or
// interprets it: a value read from a file is deep-copied and written back unchanged on save. It lives
// outside the edited data, so it is not part of dirty state or undo/redo snapshots.
class DocumentGenMeta
{
public:
    DocumentGenMeta() = default;
    DocumentGenMeta(const DocumentGenMeta& other) { *this = other; }
    DocumentGenMeta& operator=(const DocumentGenMeta& other)
    {
        if (this == &other)
            return *this;
        if (other.m_present)
            assign(other.m_value);
        else
            clear();
        return *this;
    }

    // Replaces the stored value with root[kDocumentGenMetaKey], or clears it when the key is absent.
    void readFrom(const rapidjson::Value& root)
    {
        clear();
        if (!root.IsObject())
            return;
        const auto it = root.FindMember(kDocumentGenMetaKey);
        if (it == root.MemberEnd())
            return;
        assign(it->value);
    }

    void clear()
    {
        rapidjson::Document empty;
        m_value.Swap(empty);
        m_present = false;
    }

    bool hasValue() const { return m_present; }
    const rapidjson::Value& value() const { return m_value; }

    // Writes the key and the stored value; writes nothing when no value was read.
    template <typename Writer>
    void writeTo(Writer& writer) const
    {
        if (!m_present)
            return;
        writer.Key(kDocumentGenMetaKey);
        m_value.Accept(writer);
    }

private:
    // Deep-copies into a fresh document so repeated loads do not grow the old allocator's pool.
    void assign(const rapidjson::Value& source)
    {
        rapidjson::Document copy;
        copy.CopyFrom(source, copy.GetAllocator(), true);
        m_value.Swap(copy);
        m_present = true;
    }

    rapidjson::Document m_value;
    bool m_present = false;
};
}  // namespace editor
