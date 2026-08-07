#include "documents/AniDocument.h"

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
using JsonValue  = rapidjson::Value;
using JsonWriter = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

bool hasString(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsString();
}

bool hasBool(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsBool();
}

bool hasInt(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsInt();
}

bool hasNumber(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsNumber();
}

bool hasObject(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsObject();
}

bool hasArray(const JsonValue& object, const char* key)
{
    return object.IsObject() && object.HasMember(key) && object[key].IsArray();
}

std::string stringValue(const JsonValue& object, const char* key)
{
    return object[key].GetString();
}

SceneVec2 readVec2(const JsonValue& value)
{
    return {value["x"].GetFloat(), value["y"].GetFloat()};
}

SceneColor readColor(const JsonValue& value)
{
    return {std::clamp(value["r"].GetFloat(), 0.0f, 1.0f), std::clamp(value["g"].GetFloat(), 0.0f, 1.0f),
            std::clamp(value["b"].GetFloat(), 0.0f, 1.0f), std::clamp(value["a"].GetFloat(), 0.0f, 1.0f)};
}

bool isVec2(const JsonValue& value)
{
    return value.IsObject() && hasNumber(value, "x") && hasNumber(value, "y");
}

bool isColor(const JsonValue& value)
{
    return value.IsObject() && hasNumber(value, "r") && hasNumber(value, "g") && hasNumber(value, "b") &&
           hasNumber(value, "a");
}

bool validPreviewVariableName(const std::string& name)
{
    return !name.empty() && name.find('{') == std::string::npos && name.find('}') == std::string::npos;
}

void writeString(JsonWriter& writer, const char* key, const std::string& value)
{
    writer.Key(key);
    writer.String(value.c_str());
}

void writeVec2(JsonWriter& writer, const char* key, const SceneVec2& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("x");
    writer.Double(value.x);
    writer.Key("y");
    writer.Double(value.y);
    writer.EndObject();
}

void writeColor(JsonWriter& writer, const char* key, const SceneColor& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("r");
    writer.Double(std::clamp(value.r, 0.0f, 1.0f));
    writer.Key("g");
    writer.Double(std::clamp(value.g, 0.0f, 1.0f));
    writer.Key("b");
    writer.Double(std::clamp(value.b, 0.0f, 1.0f));
    writer.Key("a");
    writer.Double(std::clamp(value.a, 0.0f, 1.0f));
    writer.EndObject();
}
}  // namespace

bool AniDocument::open(const std::filesystem::path& path)
{
    m_lastError.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_lastError = "Failed to open animation file: " + path.generic_string();
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!loadFromJson(buffer.str()))
        return false;

    m_path           = path.lexically_normal();
    m_savedAnimation = m_animation;
    m_undoCheckpoint = m_animation;
    m_undoStack.clear();
    m_redoStack.clear();
    ++m_revision;
    return true;
}

bool AniDocument::save()
{
    m_lastError.clear();
    if (m_path.empty())
    {
        m_lastError = "Animation document path is empty.";
        return false;
    }
    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        m_lastError = "Failed to open animation file for writing: " + m_path.generic_string();
        return false;
    }
    file << toJson();
    if (!file)
    {
        m_lastError = "Failed to write animation file: " + m_path.generic_string();
        return false;
    }
    m_savedAnimation = m_animation;
    return true;
}

bool AniDocument::isDirty() const
{
    return m_animation != m_savedAnimation;
}
std::string AniDocument::getDisplayName() const
{
    return m_path.empty() ? "Untitled.ani" : m_path.filename().generic_string();
}
const std::filesystem::path& AniDocument::path() const
{
    return m_path;
}
std::string AniDocument::lastError() const
{
    return m_lastError;
}
bool AniDocument::canUndo() const
{
    return !m_undoStack.empty();
}

bool AniDocument::undo()
{
    if (m_editTransactionActive)
        cancelEditTransaction();
    if (m_undoStack.empty())
        return false;
    m_redoStack.push_back(m_animation);
    m_animation = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    m_undoCheckpoint = m_animation;
    ++m_revision;
    return true;
}

bool AniDocument::canRedo() const
{
    return !m_redoStack.empty();
}

bool AniDocument::redo()
{
    if (m_editTransactionActive)
        cancelEditTransaction();
    if (m_redoStack.empty())
        return false;
    m_undoStack.push_back(m_animation);
    m_animation = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    m_undoCheckpoint = m_animation;
    ++m_revision;
    return true;
}

bool AniDocument::createDefaultFile(const std::filesystem::path& path, std::string& message)
{
    AniDocument document;
    document.m_path = path.lexically_normal();
    if (!document.save())
    {
        message = document.lastError();
        return false;
    }
    message = "Created animation file.";
    return true;
}

FrameAnimationData& AniDocument::animation()
{
    return m_animation;
}
const FrameAnimationData& AniDocument::animation() const
{
    return m_animation;
}

bool AniDocument::resolveTemplatePath(const std::string& templatePath,
                                      std::string& resolvedPath,
                                      std::string& error) const
{
    return resolveTemplatePath(templatePath, m_animation.previewVars, resolvedPath, error);
}

bool AniDocument::resolveTemplatePath(const std::string& templatePath,
                                      const AnimationPreviewVariables& variables,
                                      std::string& resolvedPath,
                                      std::string& error) const
{
    resolvedPath.clear();
    error.clear();
    if (templatePath.empty())
    {
        error = "Template image path is empty.";
        return false;
    }

    for (std::size_t index = 0; index < templatePath.size();)
    {
        const std::size_t open  = templatePath.find('{', index);
        const std::size_t close = templatePath.find('}', index);
        if (close != std::string::npos && (open == std::string::npos || close < open))
        {
            error = "Template image path contains an unmatched '}'.";
            return false;
        }
        if (open == std::string::npos)
        {
            resolvedPath.append(templatePath, index, std::string::npos);
            break;
        }

        resolvedPath.append(templatePath, index, open - index);
        const std::size_t end = templatePath.find('}', open + 1);
        if (end == std::string::npos)
        {
            error = "Template image path contains an unmatched '{'.";
            return false;
        }
        const std::string key = templatePath.substr(open + 1, end - open - 1);
        if (!validPreviewVariableName(key))
        {
            error = "Template variable name is invalid.";
            return false;
        }
        const auto it = variables.find(key);
        if (it == variables.end())
        {
            error = "Missing preview variable: " + key;
            return false;
        }
        resolvedPath += it->second;
        index = end + 1;
    }
    if (resolvedPath.empty())
    {
        error = "Resolved template image path is empty.";
        return false;
    }
    if (resolvedPath.find('{') != std::string::npos || resolvedPath.find('}') != std::string::npos)
    {
        error = "Resolved template image path still contains an unresolved placeholder.";
        return false;
    }
    return true;
}

bool AniDocument::duplicateFrame(std::size_t frameIndex)
{
    if (frameIndex >= m_animation.frames.size())
        return false;
    AnimationFrameData duplicate = m_animation.frames[frameIndex];
    m_animation.frames.insert(m_animation.frames.begin() + static_cast<std::ptrdiff_t>(frameIndex + 1),
                              std::move(duplicate));
    markDirty();
    return true;
}

bool AniDocument::beginEditTransaction()
{
    if (m_editTransactionActive)
        return false;
    m_transactionSnapshot   = m_animation;
    m_editTransactionActive = true;
    return true;
}

void AniDocument::commitEditTransaction()
{
    if (!m_editTransactionActive)
        return;
    if (m_animation != m_transactionSnapshot)
    {
        pushUndoSnapshot(m_transactionSnapshot);
        m_undoCheckpoint = m_animation;
        ++m_revision;
    }
    m_editTransactionActive = false;
}

void AniDocument::cancelEditTransaction()
{
    if (!m_editTransactionActive)
        return;
    m_animation             = m_transactionSnapshot;
    m_undoCheckpoint        = m_animation;
    m_editTransactionActive = false;
    ++m_revision;
}

void AniDocument::markDirty()
{
    if (m_editTransactionActive)
    {
        ++m_revision;
        return;
    }
    if (m_animation == m_undoCheckpoint)
        return;
    pushUndoSnapshot(m_undoCheckpoint);
    m_undoCheckpoint = m_animation;
    ++m_revision;
}

void AniDocument::pushUndoSnapshot(const FrameAnimationData& snapshot)
{
    constexpr std::size_t kMaxUndoEntries = 100;
    if (m_undoStack.size() >= kMaxUndoEntries)
        m_undoStack.erase(m_undoStack.begin());
    m_undoStack.push_back(snapshot);
    m_redoStack.clear();
}

std::size_t AniDocument::revision() const
{
    return m_revision;
}

bool AniDocument::loadFromJson(const std::string& json)
{
    rapidjson::Document root;
    root.Parse(json.c_str());
    if (root.HasParseError())
    {
        m_lastError = std::string("Invalid animation JSON: ") + rapidjson::GetParseError_En(root.GetParseError());
        return false;
    }
    if (!root.IsObject())
    {
        m_lastError = "Animation root must be an object.";
        return false;
    }
    if (!hasInt(root, "version") || root["version"].GetInt() != FrameAnimationData::kCurrentVersion)
    {
        const int version = hasInt(root, "version") ? root["version"].GetInt() : -1;
        m_lastError       = "Unsupported animation version: " + std::to_string(version);
        return false;
    }
    if (!hasBool(root, "loop") || !hasArray(root, "frames"))
    {
        m_lastError = "Animation root is missing required V1 fields.";
        return false;
    }
    // Migrated empty previewVars was sometimes written as [] because Lua `{}` is ambiguous.
    if (hasObject(root, "previewVars"))
    {
        // ok
    }
    else if (hasArray(root, "previewVars") && root["previewVars"].Empty())
    {
        // treat empty array as empty object
    }
    else
    {
        m_lastError = "Animation root is missing required V1 fields.";
        return false;
    }

    FrameAnimationData parsed;
    parsed.loop = root["loop"].GetBool();
    if (root["previewVars"].IsObject())
    {
        for (auto it = root["previewVars"].MemberBegin(); it != root["previewVars"].MemberEnd(); ++it)
        {
            if (!it->name.IsString() || !it->value.IsString())
            {
                m_lastError = "Preview variables must contain string keys and values.";
                return false;
            }
            const std::string key = it->name.GetString();
            if (!validPreviewVariableName(key))
            {
                m_lastError = "Preview variable name is invalid: " + key;
                return false;
            }
            parsed.previewVars[key] = it->value.GetString();
        }
    }
    for (const JsonValue& value : root["frames"].GetArray())
    {
        if (!value.IsObject() || !hasString(value, "sourceType") || !hasInt(value, "delay") ||
            value["delay"].GetInt() < 1 || !hasObject(value, "transform"))
        {
            m_lastError = "Animation frame is missing required V1 fields.";
            return false;
        }

        AnimationFrameData frame;
        const std::string sourceType = stringValue(value, "sourceType");
        if (sourceType == "Texture")
        {
            if (!hasString(value, "path") || stringValue(value, "path").empty())
            {
                m_lastError = "Texture animation frame path is empty.";
                return false;
            }
            frame.path = stringValue(value, "path");
        }
        else if (sourceType == "SpriteFrame")
        {
            if (!hasString(value, "atlasPath") || !hasString(value, "frameName") ||
                stringValue(value, "atlasPath").empty() || stringValue(value, "frameName").empty())
            {
                m_lastError = "SpriteFrame animation frame source is incomplete.";
                return false;
            }
            frame.sourceType = AnimationFrameSourceType::SpriteFrame;
            frame.atlasPath  = stringValue(value, "atlasPath");
            frame.frameName  = stringValue(value, "frameName");
        }
        else if (sourceType == "Template")
        {
            if (!hasString(value, "image") || stringValue(value, "image").empty())
            {
                m_lastError = "Template animation frame image path is empty.";
                return false;
            }
            frame.sourceType   = AnimationFrameSourceType::Template;
            frame.templatePath = stringValue(value, "image");
        }
        else
        {
            m_lastError = "Unknown animation frame source type: " + sourceType;
            return false;
        }
        frame.delayMs = value["delay"].GetInt();

        const JsonValue& transform = value["transform"];
        if (!hasObject(transform, "offset") || !isVec2(transform["offset"]) || !hasObject(transform, "scale") ||
            !isVec2(transform["scale"]) || !hasNumber(transform, "rotation") || !hasObject(transform, "color") ||
            !isColor(transform["color"]))
        {
            m_lastError = "Animation frame transform is missing required fields.";
            return false;
        }
        if (hasObject(transform, "anchor") && !isVec2(transform["anchor"]))
        {
            m_lastError = "Animation frame transform.anchor must be a vec2.";
            return false;
        }
        frame.offset   = readVec2(transform["offset"]);
        frame.anchor   = hasObject(transform, "anchor") ? readVec2(transform["anchor"]) : SceneVec2{0.5f, 0.5f};
        frame.scale    = readVec2(transform["scale"]);
        frame.rotation = transform["rotation"].GetFloat();
        frame.color    = readColor(transform["color"]);

        parsed.frames.push_back(std::move(frame));
    }

    m_animation = std::move(parsed);
    return true;
}

std::string AniDocument::toJson() const
{
    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    writer.Key("version");
    writer.Int(FrameAnimationData::kCurrentVersion);
    writer.Key("loop");
    writer.Bool(m_animation.loop);
    writer.Key("previewVars");
    writer.StartObject();
    for (const auto& [key, value] : m_animation.previewVars)
        writeString(writer, key.c_str(), value);
    writer.EndObject();
    writer.Key("frames");
    writer.StartArray();
    for (const AnimationFrameData& frame : m_animation.frames)
    {
        writer.StartObject();
        if (frame.sourceType == AnimationFrameSourceType::SpriteFrame)
        {
            writeString(writer, "sourceType", "SpriteFrame");
            writeString(writer, "atlasPath", frame.atlasPath);
            writeString(writer, "frameName", frame.frameName);
        }
        else if (frame.sourceType == AnimationFrameSourceType::Template)
        {
            writeString(writer, "sourceType", "Template");
            writeString(writer, "image", frame.templatePath);
        }
        else
        {
            writeString(writer, "sourceType", "Texture");
            writeString(writer, "path", frame.path);
        }
        writer.Key("delay");
        writer.Int(std::max(1, frame.delayMs));
        writer.Key("transform");
        writer.StartObject();
        writeVec2(writer, "offset", frame.offset);
        writeVec2(writer, "anchor", frame.anchor);
        writeVec2(writer, "scale", frame.scale);
        writer.Key("rotation");
        writer.Double(frame.rotation);
        writeColor(writer, "color", frame.color);
        writer.EndObject();
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
    return buffer.GetString();
}
}  // namespace editor
