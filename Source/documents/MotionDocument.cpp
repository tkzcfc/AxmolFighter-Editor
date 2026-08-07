#include "documents/MotionDocument.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

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

bool hasArray(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsArray();
}

bool validateData(const MotionData& data, std::string& error)
{
    if (data.version != MotionData::kCurrentVersion)
    {
        error = "Motion data has an invalid version.";
        return false;
    }

    std::unordered_set<std::string> motionNames;
    for (const MotionEntry& motion : data.motions)
    {
        if (motion.name.empty())
        {
            error = "Motion name cannot be empty.";
            return false;
        }
        if (!motionNames.insert(motion.name).second)
        {
            error = "Duplicate motion name: " + motion.name;
            return false;
        }

        std::unordered_set<std::string> animationIds;
        for (const MotionAnimationEntry& entry : motion.animations)
        {
            if (entry.id.empty())
            {
                error = "Animation id cannot be empty in motion: " + motion.name;
                return false;
            }
            if (entry.source.empty())
            {
                error = "Animation source cannot be empty in motion: " + motion.name;
                return false;
            }
            if (entry.type != MotionAnimationType::Ani && entry.type != MotionAnimationType::Spine)
            {
                error = "Animation type must be ani or spine in motion: " + motion.name;
                return false;
            }
            if (!animationIds.insert(entry.id).second)
            {
                error = "Duplicate animation id \"" + entry.id + "\" in motion: " + motion.name;
                return false;
            }
        }
    }
    return true;
}
}  // namespace

bool MotionDocument::open(const std::filesystem::path& path)
{
    m_lastError.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_lastError = "Failed to open motion file: " + path.generic_string();
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

bool MotionDocument::save()
{
    m_lastError.clear();
    if (!validateData(m_data, m_lastError))
        return false;
    if (m_path.empty())
    {
        m_lastError = "Motion document path is empty.";
        return false;
    }

    sortMotionsByName();

    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        m_lastError = "Failed to open motion file for writing: " + m_path.generic_string();
        return false;
    }
    file << toJson();
    if (!file)
    {
        m_lastError = "Failed to write motion file: " + m_path.generic_string();
        return false;
    }
    m_savedData = m_data;
    return true;
}

bool MotionDocument::isDirty() const { return m_data != m_savedData; }

std::string MotionDocument::getDisplayName() const
{
    return m_path.empty() ? "Untitled.motion" : m_path.filename().generic_string();
}

const std::filesystem::path& MotionDocument::path() const { return m_path; }

std::string MotionDocument::lastError() const { return m_lastError; }

bool MotionDocument::canUndo() const { return !m_undoStack.empty(); }

bool MotionDocument::undo()
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

bool MotionDocument::canRedo() const { return !m_redoStack.empty(); }

bool MotionDocument::redo()
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

bool MotionDocument::createDefaultFile(const std::filesystem::path& path, std::string& message)
{
    MotionDocument document;
    document.m_path = path.lexically_normal();
    if (!document.save())
    {
        message = document.lastError();
        return false;
    }
    message = "Created motion file.";
    return true;
}

MotionData& MotionDocument::data() { return m_data; }
const MotionData& MotionDocument::data() const { return m_data; }

bool MotionDocument::beginEditTransaction()
{
    if (m_editTransactionActive)
        return false;
    m_transactionSnapshot = m_data;
    m_editTransactionActive = true;
    return true;
}

void MotionDocument::commitEditTransaction()
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

void MotionDocument::cancelEditTransaction()
{
    if (!m_editTransactionActive)
        return;
    m_data = m_transactionSnapshot;
    m_undoCheckpoint = m_data;
    m_editTransactionActive = false;
    ++m_revision;
}

void MotionDocument::markDirty()
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

std::size_t MotionDocument::revision() const { return m_revision; }

bool MotionDocument::loadFromJson(const std::string& json)
{
    rapidjson::Document root;
    root.Parse(json.c_str());
    if (root.HasParseError())
    {
        m_lastError = std::string("Invalid motion JSON: ") + rapidjson::GetParseError_En(root.GetParseError());
        return false;
    }
    if (!root.IsObject() || !hasString(root, "type") || std::string(root["type"].GetString()) != "motion")
    {
        m_lastError = "Motion root type must be motion.";
        return false;
    }
    if (!hasInt(root, "version") || root["version"].GetInt() != MotionData::kCurrentVersion)
    {
        const int version = hasInt(root, "version") ? root["version"].GetInt() : -1;
        m_lastError = "Unsupported motion version: " + std::to_string(version);
        return false;
    }
    if (!hasArray(root, "motions"))
    {
        m_lastError = "Motion root is missing required V1 fields.";
        return false;
    }

    MotionData parsed;
    for (const JsonValue& value : root["motions"].GetArray())
    {
        if (!value.IsObject() || !hasString(value, "name") || !hasArray(value, "animations"))
        {
            m_lastError = "Motion entry is missing required V1 fields.";
            return false;
        }

        MotionEntry motion;
        motion.name = value["name"].GetString();
        for (const JsonValue& animationValue : value["animations"].GetArray())
        {
            if (!animationValue.IsObject() || !hasString(animationValue, "id") || !hasString(animationValue, "type") ||
                !hasString(animationValue, "source"))
            {
                m_lastError = "Motion animation entry is missing required fields (id/type/source).";
                return false;
            }
            const std::string typeStr = animationValue["type"].GetString();
            MotionAnimationType entryType = MotionAnimationType::Ani;
            if (typeStr == "ani")
                entryType = MotionAnimationType::Ani;
            else if (typeStr == "spine")
                entryType = MotionAnimationType::Spine;
            else
            {
                m_lastError = "Motion animation type must be ani or spine.";
                return false;
            }
            if (animationValue["source"].GetStringLength() == 0)
            {
                m_lastError = "Motion animation source cannot be empty.";
                return false;
            }
            if (animationValue.HasMember("box") && !animationValue["box"].IsString())
            {
                m_lastError = "Motion animation box must be a string.";
                return false;
            }
            if (animationValue.HasMember("tag") && !animationValue["tag"].IsString())
            {
                m_lastError = "Motion animation tag must be a string.";
                return false;
            }

            MotionAnimationEntry entry;
            entry.id     = animationValue["id"].GetString();
            entry.type   = entryType;
            entry.source = animationValue["source"].GetString();
            if (animationValue.HasMember("box"))
                entry.box = animationValue["box"].GetString();
            if (animationValue.HasMember("tag"))
                entry.tag = animationValue["tag"].GetString();
            motion.animations.push_back(std::move(entry));
        }
        parsed.motions.push_back(std::move(motion));
    }

    if (!validateData(parsed, m_lastError))
        return false;

    m_data = std::move(parsed);
    return true;
}

std::string MotionDocument::toJson() const
{
    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    writer.Key("type");
    writer.String("motion");
    writer.Key("version");
    writer.Int(MotionData::kCurrentVersion);
    writer.Key("motions");
    writer.StartArray();
    for (const MotionEntry& motion : m_data.motions)
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(motion.name.c_str());
        writer.Key("animations");
        writer.StartArray();
        for (const MotionAnimationEntry& entry : motion.animations)
        {
            writer.StartObject();
            writer.Key("id");
            writer.String(entry.id.c_str());
            writer.Key("type");
            writer.String(entry.type == MotionAnimationType::Spine ? "spine" : "ani");
            writer.Key("source");
            writer.String(entry.source.c_str());
            writer.Key("box");
            writer.String(entry.box.c_str());
            if (!entry.tag.empty())
            {
                writer.Key("tag");
                writer.String(entry.tag.c_str());
            }
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
    return buffer.GetString();
}

void MotionDocument::pushUndoSnapshot(const MotionData& snapshot)
{
    constexpr std::size_t kMaxUndoEntries = 100;
    if (m_undoStack.size() >= kMaxUndoEntries)
        m_undoStack.erase(m_undoStack.begin());
    m_undoStack.push_back(snapshot);
    m_redoStack.clear();
}

void MotionDocument::sortMotionsByName()
{
    std::stable_sort(m_data.motions.begin(), m_data.motions.end(),
                     [](const MotionEntry& left, const MotionEntry& right) { return left.name < right.name; });
}
}  // namespace editor
