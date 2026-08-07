#include "documents/BoxDocument.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace editor
{
namespace
{
using JsonValue = rapidjson::Value;
using JsonWriter = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

bool hasString(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsString();
}

bool hasInt(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsInt();
}

bool hasObject(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsObject();
}

bool hasArray(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsArray();
}

const char* previewKindName(BoxPreviewKind kind)
{
    switch (kind)
    {
    case BoxPreviewKind::None:
        return "none";
    case BoxPreviewKind::Ani:
        return "ani";
    case BoxPreviewKind::Spine:
        return "spine";
    }
    return "none";
}

bool readPreviewKind(const JsonValue& value, BoxPreviewKind& result)
{
    if (!value.IsString())
        return false;
    const std::string kind = value.GetString();
    if (kind == "none")
        result = BoxPreviewKind::None;
    else if (kind == "ani")
        result = BoxPreviewKind::Ani;
    else if (kind == "spine")
        result = BoxPreviewKind::Spine;
    else
        return false;
    return true;
}

bool readPreview(const JsonValue& value, BoxPreviewData& result)
{
    if (!value.IsObject() || !value.HasMember("kind") || !readPreviewKind(value["kind"], result.kind) ||
        !hasString(value, "path") || !hasString(value, "atlasPath") || !hasString(value, "animation"))
        return false;

    result.path = value["path"].GetString();
    result.atlasPath = value["atlasPath"].GetString();
    result.animation = value["animation"].GetString();
    if (result.kind == BoxPreviewKind::None)
        return result.path.empty() && result.atlasPath.empty() && result.animation.empty();
    if (result.kind == BoxPreviewKind::Ani)
        return !result.path.empty() && result.atlasPath.empty() && result.animation.empty();
    return !result.path.empty() && !result.atlasPath.empty() && !result.animation.empty();
}

void writePreview(JsonWriter& writer, const BoxPreviewData& preview)
{
    writer.Key("preview");
    writer.StartObject();
    writer.Key("kind");
    writer.String(previewKindName(preview.kind));
    writer.Key("path");
    writer.String(preview.path.c_str());
    writer.Key("atlasPath");
    writer.String(preview.atlasPath.c_str());
    writer.Key("animation");
    writer.String(preview.animation.c_str());
    writer.EndObject();
}

bool readVec3(const JsonValue& value, Vec3i& result)
{
    if (!value.IsObject() || !hasInt(value, "x") || !hasInt(value, "y") || !hasInt(value, "z"))
        return false;
    result = {value["x"].GetInt(), value["y"].GetInt(), value["z"].GetInt()};
    return true;
}

void writeVec3(JsonWriter& writer, const char* key, const Vec3i& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("x");
    writer.Int(value.x);
    writer.Key("y");
    writer.Int(value.y);
    writer.Key("z");
    writer.Int(value.z);
    writer.EndObject();
}

bool readBox(const JsonValue& value, CombatBox& result)
{
    if (!value.IsObject() || !hasObject(value, "pos") || !hasObject(value, "size") ||
        !readVec3(value["pos"], result.pos) || !readVec3(value["size"], result.size))
        return false;
    return result.size.x > 0 && result.size.y > 0 && result.size.z > 0;
}

void writeBox(JsonWriter& writer, const CombatBox& value)
{
    writer.StartObject();
    writeVec3(writer, "pos", value.pos);
    writeVec3(writer, "size", value.size);
    writer.EndObject();
}

bool readKey(const JsonValue& value, BoxKey& result)
{
    if (!value.IsObject() || !hasInt(value, "timeMs"))
        return false;
    result.timeMs = value["timeMs"].GetInt();
    // Older migrate output omitted cleared boxes entirely; treat missing as null.
    if (!value.HasMember("box") || value["box"].IsNull())
    {
        result.box.reset();
        return true;
    }
    CombatBox box;
    if (!readBox(value["box"], box))
        return false;
    result.box = box;
    return true;
}

void writeKey(JsonWriter& writer, const BoxKey& value)
{
    writer.StartObject();
    writer.Key("timeMs");
    writer.Int(value.timeMs);
    writer.Key("box");
    if (value.box)
        writeBox(writer, *value.box);
    else
        writer.Null();
    writer.EndObject();
}

void normalizeTrackKeys(BoxTrack& track, std::int32_t duration)
{
    track.keys.erase(std::remove_if(track.keys.begin(), track.keys.end(), [duration](const BoxKey& key) {
                         return key.timeMs < 0 || key.timeMs >= duration;
                     }),
                     track.keys.end());
    std::stable_sort(track.keys.begin(), track.keys.end(), [](const BoxKey& left, const BoxKey& right) {
        return left.timeMs < right.timeMs;
    });
    std::vector<BoxKey> normalized;
    normalized.reserve(track.keys.size());
    for (const BoxKey& key : track.keys)
    {
        if (!normalized.empty() && normalized.back().timeMs == key.timeMs)
            normalized.back() = key;
        else
            normalized.push_back(key);
    }
    track.keys = std::move(normalized);
}

bool validateData(const BoxData& data, std::string& error)
{
    if (data.version != BoxData::kCurrentVersion || data.duration < 0)
    {
        error = "Box data has an invalid version or duration.";
        return false;
    }
    switch (data.preview.kind)
    {
    case BoxPreviewKind::None:
        if (!data.preview.path.empty() || !data.preview.atlasPath.empty() || !data.preview.animation.empty())
        {
            error = "Box data contains an invalid empty preview.";
            return false;
        }
        break;
    case BoxPreviewKind::Ani:
        if (data.preview.path.empty() || !data.preview.atlasPath.empty() || !data.preview.animation.empty())
        {
            error = "Box data contains an invalid ANI preview.";
            return false;
        }
        break;
    case BoxPreviewKind::Spine:
        if (data.preview.path.empty() || data.preview.atlasPath.empty() || data.preview.animation.empty())
        {
            error = "Box data contains an invalid Spine preview.";
            return false;
        }
        break;
    }
    for (const BoxTrack& track : data.tracks)
    {
        if (track.kind != "attack" && track.kind != "damage" && track.kind != "hitbox")
        {
            error = "Box data contains a track with an invalid kind.";
            return false;
        }
        std::int32_t previousTime = -1;
        for (const BoxKey& key : track.keys)
        {
            if (key.timeMs < 0 || key.timeMs >= data.duration || key.timeMs <= previousTime)
            {
                error = "Box data contains unsorted, duplicate, or out-of-range keys.";
                return false;
            }
            previousTime = key.timeMs;
            if (key.box && (key.box->size.x <= 0 || key.box->size.y <= 0 || key.box->size.z <= 0))
            {
                error = "Box data contains a non-positive box size.";
                return false;
            }
        }
    }
    for (const BoxEvent& event : data.events)
    {
        if (event.timeMs < 0 || event.timeMs > data.duration)
        {
            error = "Box data contains an event outside the document duration.";
            return false;
        }
    }
    return true;
}
}  // namespace

bool BoxDocument::open(const std::filesystem::path& path)
{
    m_lastError.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_lastError = "Failed to open box file: " + path.generic_string();
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!loadFromJson(buffer.str()))
        return false;

    m_path = path.lexically_normal();
    m_savedData = m_data;
    m_undoCheckpoint = m_data;
    m_undoStack.clear();
    m_redoStack.clear();
    m_editTransactionActive = false;
    ++m_revision;
    return true;
}

bool BoxDocument::save()
{
    m_lastError.clear();
    if (!validateData(m_data, m_lastError))
        return false;
    if (m_path.empty())
    {
        m_lastError = "Box document path is empty.";
        return false;
    }
    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        m_lastError = "Failed to open box file for writing: " + m_path.generic_string();
        return false;
    }
    file << toJson();
    if (!file)
    {
        m_lastError = "Failed to write box file: " + m_path.generic_string();
        return false;
    }
    m_savedData = m_data;
    return true;
}

bool BoxDocument::isDirty() const { return m_data != m_savedData; }

std::string BoxDocument::getDisplayName() const
{
    return m_path.empty() ? "Untitled.box" : m_path.filename().generic_string();
}

const std::filesystem::path& BoxDocument::path() const { return m_path; }

std::string BoxDocument::lastError() const { return m_lastError; }

bool BoxDocument::canUndo() const { return !m_undoStack.empty(); }

bool BoxDocument::undo()
{
    if (m_editTransactionActive)
        cancelEditTransaction();
    if (m_undoStack.empty())
        return false;
    m_redoStack.push_back(m_data);
    m_data = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    m_undoCheckpoint = m_data;
    ++m_revision;
    return true;
}

bool BoxDocument::canRedo() const { return !m_redoStack.empty(); }

bool BoxDocument::redo()
{
    if (m_editTransactionActive)
        cancelEditTransaction();
    if (m_redoStack.empty())
        return false;
    m_undoStack.push_back(m_data);
    m_data = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    m_undoCheckpoint = m_data;
    ++m_revision;
    return true;
}

bool BoxDocument::createDefaultFile(const std::filesystem::path& path, std::string& message)
{
    BoxDocument document;
    document.m_path = path.lexically_normal();
    if (!document.save())
    {
        message = document.lastError();
        return false;
    }
    message = "Created box file.";
    return true;
}

BoxData& BoxDocument::data() { return m_data; }
const BoxData& BoxDocument::data() const { return m_data; }

bool BoxDocument::beginEditTransaction()
{
    if (m_editTransactionActive)
        return false;
    m_transactionSnapshot = m_data;
    m_editTransactionActive = true;
    return true;
}

void BoxDocument::commitEditTransaction()
{
    if (!m_editTransactionActive)
        return;
    if (m_data != m_transactionSnapshot)
    {
        pushUndoSnapshot(m_transactionSnapshot);
        m_undoCheckpoint = m_data;
        ++m_revision;
    }
    m_editTransactionActive = false;
}

void BoxDocument::cancelEditTransaction()
{
    if (!m_editTransactionActive)
        return;
    m_data = m_transactionSnapshot;
    m_undoCheckpoint = m_data;
    m_editTransactionActive = false;
    ++m_revision;
}

void BoxDocument::markDirty()
{
    if (m_editTransactionActive)
    {
        ++m_revision;
        return;
    }
    if (m_data == m_undoCheckpoint)
        return;
    pushUndoSnapshot(m_undoCheckpoint);
    m_undoCheckpoint = m_data;
    ++m_revision;
}

void BoxDocument::normalizeToDuration()
{
    m_data.duration = std::max<std::int32_t>(0, m_data.duration);
    normalizeKeys();
    for (BoxEvent& event : m_data.events)
        event.timeMs = std::clamp(event.timeMs, 0, m_data.duration);
}

void BoxDocument::normalizeKeys()
{
    for (BoxTrack& track : m_data.tracks)
        normalizeTrackKeys(track, m_data.duration);
}

std::size_t BoxDocument::revision() const { return m_revision; }

bool BoxDocument::loadFromJson(const std::string& json)
{
    rapidjson::Document root;
    root.Parse(json.c_str());
    if (root.HasParseError())
    {
        m_lastError = std::string("Invalid box JSON: ") + rapidjson::GetParseError_En(root.GetParseError());
        return false;
    }
    if (!root.IsObject() || !hasString(root, "type") || std::string(root["type"].GetString()) != "combat")
    {
        m_lastError = "Box root type must be combat.";
        return false;
    }
    if (!hasInt(root, "version") || root["version"].GetInt() != BoxData::kCurrentVersion)
    {
        const int version = hasInt(root, "version") ? root["version"].GetInt() : -1;
        m_lastError = "Unsupported box version: " + std::to_string(version);
        return false;
    }
    if (!hasInt(root, "duration") || !hasArray(root, "tracks") || !hasArray(root, "events") ||
        !root.HasMember("preview") || !root["preview"].IsObject())
    {
        m_lastError = "Box root is missing required V1 fields.";
        return false;
    }

    BoxData parsed;
    parsed.duration = root["duration"].GetInt();
    if (!readPreview(root["preview"], parsed.preview))
    {
        m_lastError = "Box preview is missing required V1 fields or is invalid.";
        return false;
    }
    if (parsed.duration < 0)
    {
        m_lastError = "Box duration cannot be negative.";
        return false;
    }

    for (const JsonValue& value : root["tracks"].GetArray())
    {
        if (!value.IsObject() || !hasString(value, "name") || !hasString(value, "kind") ||
            !hasArray(value, "keys"))
        {
            m_lastError = "Box track is missing required V1 fields.";
            return false;
        }
        BoxTrack track;
        track.name = value["name"].GetString();
        track.kind = value["kind"].GetString();
        if (track.kind != "attack" && track.kind != "damage" && track.kind != "hitbox")
        {
            m_lastError = "Box track has an invalid kind.";
            return false;
        }
        std::int32_t previousTime = -1;
        for (const JsonValue& keyValue : value["keys"].GetArray())
        {
            BoxKey key;
            if (!readKey(keyValue, key))
            {
                m_lastError = "Box track contains an invalid key.";
                return false;
            }
            if (key.timeMs < 0 || key.timeMs >= parsed.duration)
            {
                m_lastError = "Box track key time is outside [0, duration).";
                return false;
            }
            if (key.timeMs <= previousTime)
            {
                m_lastError = "Box track contains duplicate or unsorted keys.";
                return false;
            }
            previousTime = key.timeMs;
            track.keys.push_back(std::move(key));
        }
        parsed.tracks.push_back(std::move(track));
    }

    for (const JsonValue& value : root["events"].GetArray())
    {
        if (!value.IsObject() || !hasInt(value, "timeMs") || !hasString(value, "type") ||
            (value.HasMember("value") && !value["value"].IsString()))
        {
            m_lastError = "Box event is missing required V1 fields.";
            return false;
        }
        BoxEvent event;
        event.timeMs = value["timeMs"].GetInt();
        event.type = value["type"].GetString();
        if (value.HasMember("value"))
            event.value = value["value"].GetString();
        if (event.timeMs < 0 || event.timeMs > parsed.duration)
        {
            m_lastError = "Box event time is outside the document duration.";
            return false;
        }
        parsed.events.push_back(std::move(event));
    }

    m_data = std::move(parsed);
    return true;
}

std::string BoxDocument::toJson() const
{
    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    writer.Key("type");
    writer.String("combat");
    writer.Key("version");
    writer.Int(BoxData::kCurrentVersion);
    writer.Key("duration");
    writer.Int(m_data.duration);
    writePreview(writer, m_data.preview);
    writer.Key("tracks");
    writer.StartArray();
    for (const BoxTrack& track : m_data.tracks)
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(track.name.c_str());
        writer.Key("kind");
        writer.String(track.kind.c_str());
        writer.Key("keys");
        writer.StartArray();
        for (const BoxKey& key : track.keys)
            writeKey(writer, key);
        writer.EndArray();
        writer.EndObject();
    }
    writer.EndArray();
    writer.Key("events");
    writer.StartArray();
    for (const BoxEvent& event : m_data.events)
    {
        writer.StartObject();
        writer.Key("timeMs");
        writer.Int(event.timeMs);
        writer.Key("type");
        writer.String(event.type.c_str());
        if (!event.value.empty())
        {
            writer.Key("value");
            writer.String(event.value.c_str());
        }
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
    return buffer.GetString();
}

void BoxDocument::pushUndoSnapshot(const BoxData& snapshot)
{
    constexpr std::size_t kMaxUndoEntries = 100;
    if (m_undoStack.size() >= kMaxUndoEntries)
        m_undoStack.erase(m_undoStack.begin());
    m_undoStack.push_back(snapshot);
    m_redoStack.clear();
}
}  // namespace editor
