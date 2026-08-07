#include "core/EditorPreferencesService.h"

#include "core/ProjectSettings.h"
#include "platform/Common.h"
#include "platform/FileUtils.h"
#include "base/UserDefault.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
namespace
{
constexpr const char* kEditorPreferencesKey = "editor.preferences.v1";
constexpr const char* kAssetBrowserStateKey = "editor.assetbrowser.v1";

constexpr SceneColor kDefaultRootBoundsColor = {104.0f / 255.0f, 180.0f / 255.0f, 1.0f, 210.0f / 255.0f};
constexpr SceneColor kDefaultObjectNoteColor = {0.26f, 0.59f, 0.98f, 1.0f};
constexpr SceneColor kDefaultObjectNoteOutlineColor = {0.05f, 0.06f, 0.08f, 1.0f};
constexpr SceneColor kDefaultShapeFillColor = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr SceneColor kDefaultShapeStrokeColor = {104.0f / 255.0f, 180.0f / 255.0f, 1.0f, 1.0f};
constexpr const char* kDefaultEditorFontPath = "fonts/FZZHUNYUAN.TTF";
constexpr float kDefaultEditorFontSize = 16.0f;
constexpr float kDefaultObjectNoteFontSize = 24.0f;
constexpr float kDefaultObjectNoteOutlineSize = 2.0f;
constexpr float kMinEditorFontSize = 8.0f;
constexpr float kMaxEditorFontSize = 48.0f;

std::string normalizedPathString(const std::string& path)
{
    if (path.empty())
        return {};
    return std::filesystem::path(path).lexically_normal().generic_string();
}

bool fontFileExists(const std::string& path)
{
    if (path.empty())
        return false;

    return ax::FileUtils::getInstance()->isFileExist(path);
}

void ensureDefaultEditorFontExists()
{
    if (!fontFileExists(kDefaultEditorFontPath))
    {
        const std::string message = std::string("Required editor font is missing:\n") + kDefaultEditorFontPath +
                                    "\n\nPlease restore this file and restart the editor.";
        ax::showAlert(message, "Editor Font Missing", ax::AlertStyle::Ok | ax::AlertStyle::IconError);
        std::exit(EXIT_FAILURE);
    }
}

std::string existingEditorFontPathOrDefault(const std::string& path)
{
    const std::string normalized = normalizedPathString(path);
    if (fontFileExists(normalized))
        return normalized;

    ensureDefaultEditorFontExists();
    return kDefaultEditorFontPath;
}

float clampColor(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float clampFontSize(float value)
{
    if (!std::isfinite(value))
        return kDefaultEditorFontSize;
    return std::clamp(value, kMinEditorFontSize, kMaxEditorFontSize);
}

float clampPositive(float value, float fallback, float minimum)
{
    if (!std::isfinite(value))
        return fallback;
    return std::max(minimum, value);
}

SceneColor clampSceneColor(const SceneColor& value)
{
    return {clampColor(value.r), clampColor(value.g), clampColor(value.b), clampColor(value.a)};
}

void addSearchPathToFront(const std::string& path)
{
    ax::FileUtils* fileUtils = ax::FileUtils::getInstance();
    if (!fileUtils || path.empty())
        return;

    const std::string normalized = normalizedPathString(path);
    fileUtils->addSearchPath(normalized, true);
}

const rapidjson::Value* objectMember(const rapidjson::Value& object, const char* key)
{
    if (!object.IsObject() || !object.HasMember(key))
        return nullptr;
    return &object[key];
}

std::string jsonStringOr(const rapidjson::Value& object, const char* key, const std::string& fallback)
{
    const rapidjson::Value* value = objectMember(object, key);
    if (!value || !value->IsString())
        return fallback;
    return value->GetString();
}

float jsonFloatOr(const rapidjson::Value& object, const char* key, float fallback)
{
    const rapidjson::Value* value = objectMember(object, key);
    if (!value || !value->IsNumber())
        return fallback;
    return value->GetFloat();
}

bool jsonBoolOr(const rapidjson::Value& object, const char* key, bool fallback)
{
    const rapidjson::Value* value = objectMember(object, key);
    if (!value || !value->IsBool())
        return fallback;
    return value->GetBool();
}

SceneColor jsonColorOr(const rapidjson::Value& object, const char* key, SceneColor fallback)
{
    const rapidjson::Value* value = objectMember(object, key);
    if (!value || !value->IsObject())
        return fallback;

    return {jsonFloatOr(*value, "r", fallback.r),
            jsonFloatOr(*value, "g", fallback.g),
            jsonFloatOr(*value, "b", fallback.b),
            jsonFloatOr(*value, "a", fallback.a)};
}

EditorPreferences clampPreferences(EditorPreferences value)
{
    value.workspace.workingDirectoryOverride = normalizedPathString(value.workspace.workingDirectoryOverride);
    value.npkExporter.npkDirectory = normalizedPathString(value.npkExporter.npkDirectory);
    value.npkExporter.outputRoot   = normalizedPathString(value.npkExporter.outputRoot);
    value.editor.fontPath = existingEditorFontPathOrDefault(value.editor.fontPath);
    value.editor.fontSize = clampFontSize(value.editor.fontSize);
    value.contentView.rootBoundsColor = clampSceneColor(value.contentView.rootBoundsColor);
    value.sceneDefaults.objectNote.color = clampSceneColor(value.sceneDefaults.objectNote.color);
    value.sceneDefaults.objectNote.fontSize =
        clampPositive(value.sceneDefaults.objectNote.fontSize, kDefaultObjectNoteFontSize, 1.0f);
    value.sceneDefaults.objectNote.outlineColor = clampSceneColor(value.sceneDefaults.objectNote.outlineColor);
    value.sceneDefaults.objectNote.outlineSize =
        clampPositive(value.sceneDefaults.objectNote.outlineSize, kDefaultObjectNoteOutlineSize, 0.0f);
    value.sceneDefaults.shape.fillColor = clampSceneColor(value.sceneDefaults.shape.fillColor);
    value.sceneDefaults.shape.strokeColor = clampSceneColor(value.sceneDefaults.shape.strokeColor);
    return value;
}

void writeColorValue(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const SceneColor& color)
{
    const SceneColor clamped = clampSceneColor(color);
    writer.StartObject();
    writer.Key("r");
    writer.Double(clamped.r);
    writer.Key("g");
    writer.Double(clamped.g);
    writer.Key("b");
    writer.Double(clamped.b);
    writer.Key("a");
    writer.Double(clamped.a);
    writer.EndObject();
}

void writeColor(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const SceneColor& color)
{
    writer.Key(key);
    writeColorValue(writer, color);
}

std::string preferencesToJson(const EditorPreferences& raw)
{
    const EditorPreferences preferences = clampPreferences(raw);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();
    writer.Key("version");
    writer.Int(1);

    writer.Key("workspace");
    writer.StartObject();
    writer.Key("workingDirectoryOverride");
    writer.String(preferences.workspace.workingDirectoryOverride.c_str());
    writer.EndObject();

    writer.Key("npkExporter");
    writer.StartObject();
    writer.Key("npkDirectory");
    writer.String(preferences.npkExporter.npkDirectory.c_str());
    writer.Key("outputRoot");
    writer.String(preferences.npkExporter.outputRoot.c_str());
    writer.Key("imgFilter");
    writer.String(preferences.npkExporter.imgFilter.c_str());
    writer.EndObject();

    writer.Key("editor");
    writer.StartObject();
    writer.Key("fontPath");
    writer.String(preferences.editor.fontPath.c_str());
    writer.Key("fontSize");
    writer.Double(preferences.editor.fontSize);
    writer.EndObject();

    writer.Key("contentView");
    writer.StartObject();
    writer.Key("showRootBounds");
    writer.Bool(preferences.contentView.showRootBounds);
    writeColor(writer, "rootBoundsColor", preferences.contentView.rootBoundsColor);
    writer.EndObject();

    writer.Key("sceneDefaults");
    writer.StartObject();

    writer.Key("objectNote");
    writer.StartObject();
    writeColor(writer, "color", preferences.sceneDefaults.objectNote.color);
    writer.Key("fontSize");
    writer.Double(preferences.sceneDefaults.objectNote.fontSize);
    writer.Key("outlineEnabled");
    writer.Bool(preferences.sceneDefaults.objectNote.outlineEnabled);
    writeColor(writer, "outlineColor", preferences.sceneDefaults.objectNote.outlineColor);
    writer.Key("outlineSize");
    writer.Double(preferences.sceneDefaults.objectNote.outlineSize);
    writer.EndObject();

    writer.Key("shape");
    writer.StartObject();
    writeColor(writer, "fillColor", preferences.sceneDefaults.shape.fillColor);
    writeColor(writer, "strokeColor", preferences.sceneDefaults.shape.strokeColor);
    writer.EndObject();

    writer.EndObject();

    writer.EndObject();

    return buffer.GetString();
}

}  // namespace

EditorPreferences defaultEditorPreferences()
{
    EditorPreferences preferences;
    preferences.workspace.workingDirectoryOverride = "";
    preferences.npkExporter.npkDirectory = "";
    preferences.npkExporter.outputRoot   = "";
    preferences.npkExporter.imgFilter    = "";
    ensureDefaultEditorFontExists();
    preferences.editor.fontPath = kDefaultEditorFontPath;
    preferences.editor.fontSize = kDefaultEditorFontSize;
    preferences.contentView.showRootBounds = true;
    preferences.contentView.rootBoundsColor = kDefaultRootBoundsColor;
    preferences.sceneDefaults.objectNote.color = kDefaultObjectNoteColor;
    preferences.sceneDefaults.objectNote.fontSize = kDefaultObjectNoteFontSize;
    preferences.sceneDefaults.objectNote.outlineEnabled = true;
    preferences.sceneDefaults.objectNote.outlineColor = kDefaultObjectNoteOutlineColor;
    preferences.sceneDefaults.objectNote.outlineSize = kDefaultObjectNoteOutlineSize;
    preferences.sceneDefaults.shape.fillColor = kDefaultShapeFillColor;
    preferences.sceneDefaults.shape.strokeColor = kDefaultShapeStrokeColor;
    return preferences;
}

EditorPreferences EditorPreferencesService::preferences() const
{
    return loadPreferences();
}

EditorPreferences EditorPreferencesService::loadPreferences() const
{
    EditorPreferences preferences = defaultEditorPreferences();
    const std::string json = std::string(ax::UserDefault::getInstance()->getStringForKey(kEditorPreferencesKey, ""));
    if (json.empty())
        return preferences;

    rapidjson::Document document;
    document.Parse(json.c_str());
    if (document.HasParseError() || !document.IsObject())
        return preferences;

    if (const rapidjson::Value* workspace = objectMember(document, "workspace"))
    {
        preferences.workspace.workingDirectoryOverride =
            jsonStringOr(*workspace, "workingDirectoryOverride", preferences.workspace.workingDirectoryOverride);
    }

    if (const rapidjson::Value* npkExporter = objectMember(document, "npkExporter"))
    {
        preferences.npkExporter.npkDirectory =
            jsonStringOr(*npkExporter, "npkDirectory", preferences.npkExporter.npkDirectory);
        preferences.npkExporter.outputRoot =
            jsonStringOr(*npkExporter, "outputRoot", preferences.npkExporter.outputRoot);
        preferences.npkExporter.imgFilter =
            jsonStringOr(*npkExporter, "imgFilter", preferences.npkExporter.imgFilter);
    }

    if (const rapidjson::Value* editor = objectMember(document, "editor"))
    {
        preferences.editor.fontPath = jsonStringOr(*editor, "fontPath", preferences.editor.fontPath);
        preferences.editor.fontSize = jsonFloatOr(*editor, "fontSize", preferences.editor.fontSize);
    }

    if (const rapidjson::Value* contentView = objectMember(document, "contentView"))
    {
        preferences.contentView.showRootBounds =
            jsonBoolOr(*contentView, "showRootBounds", preferences.contentView.showRootBounds);
        preferences.contentView.rootBoundsColor =
            jsonColorOr(*contentView, "rootBoundsColor", preferences.contentView.rootBoundsColor);
    }

    if (const rapidjson::Value* sceneDefaults = objectMember(document, "sceneDefaults"))
    {
        if (const rapidjson::Value* objectNote = objectMember(*sceneDefaults, "objectNote"))
        {
            preferences.sceneDefaults.objectNote.color =
                jsonColorOr(*objectNote, "color", preferences.sceneDefaults.objectNote.color);
            preferences.sceneDefaults.objectNote.fontSize =
                jsonFloatOr(*objectNote, "fontSize", preferences.sceneDefaults.objectNote.fontSize);
            preferences.sceneDefaults.objectNote.outlineEnabled =
                jsonBoolOr(*objectNote, "outlineEnabled", preferences.sceneDefaults.objectNote.outlineEnabled);
            preferences.sceneDefaults.objectNote.outlineColor =
                jsonColorOr(*objectNote, "outlineColor", preferences.sceneDefaults.objectNote.outlineColor);
            preferences.sceneDefaults.objectNote.outlineSize =
                jsonFloatOr(*objectNote, "outlineSize", preferences.sceneDefaults.objectNote.outlineSize);
        }

        if (const rapidjson::Value* shape = objectMember(*sceneDefaults, "shape"))
        {
            preferences.sceneDefaults.shape.fillColor =
                jsonColorOr(*shape, "fillColor", preferences.sceneDefaults.shape.fillColor);
            preferences.sceneDefaults.shape.strokeColor =
                jsonColorOr(*shape, "strokeColor", preferences.sceneDefaults.shape.strokeColor);
        }
    }

    return clampPreferences(preferences);
}

void EditorPreferencesService::savePreferences(const EditorPreferences& value) const
{
    ax::UserDefault* defaults = ax::UserDefault::getInstance();
    defaults->setStringForKey(kEditorPreferencesKey, preferencesToJson(value));
    defaults->flush();
}

AssetBrowserEntry EditorPreferencesService::assetBrowserEntry(const std::string& root) const
{
    const std::string json =
        std::string(ax::UserDefault::getInstance()->getStringForKey(kAssetBrowserStateKey, ""));
    if (json.empty())
        return {};
    rapidjson::Document document;
    document.Parse(json.c_str());
    if (document.HasParseError() || !document.IsArray())
        return {};
    for (const auto& item : document.GetArray())
    {
        if (!item.IsObject() || jsonStringOr(item, "root", "") != root)
            continue;
        AssetBrowserEntry entry;
        entry.rootOpen = jsonBoolOr(item, "rootOpen", entry.rootOpen);
        if (item.HasMember("openPaths") && item["openPaths"].IsArray())
        {
            for (const auto& path : item["openPaths"].GetArray())
            {
                if (path.IsString())
                    entry.openPaths.push_back(path.GetString());
            }
        }
        return entry;
    }
    return {};
}

void EditorPreferencesService::setAssetBrowserEntry(const std::string& root, const AssetBrowserEntry& entry)
{
    struct ParsedEntry
    {
        std::string root;
        AssetBrowserEntry data;
    };
    std::vector<ParsedEntry> all;

    const std::string existingJson =
        std::string(ax::UserDefault::getInstance()->getStringForKey(kAssetBrowserStateKey, ""));
    if (!existingJson.empty())
    {
        rapidjson::Document document;
        document.Parse(existingJson.c_str());
        if (!document.HasParseError() && document.IsArray())
        {
            for (const auto& item : document.GetArray())
            {
                if (!item.IsObject())
                    continue;
                const std::string itemRoot = jsonStringOr(item, "root", "");
                if (itemRoot.empty() || itemRoot == root)
                    continue;
                AssetBrowserEntry e;
                e.rootOpen = jsonBoolOr(item, "rootOpen", e.rootOpen);
                if (item.HasMember("openPaths") && item["openPaths"].IsArray())
                {
                    for (const auto& path : item["openPaths"].GetArray())
                    {
                        if (path.IsString())
                            e.openPaths.push_back(path.GetString());
                    }
                }
                all.push_back({itemRoot, std::move(e)});
            }
        }
    }
    all.push_back({root, entry});

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartArray();
    for (const ParsedEntry& pe : all)
    {
        writer.StartObject();
        writer.Key("root");
        writer.String(pe.root.c_str());
        writer.Key("rootOpen");
        writer.Bool(pe.data.rootOpen);
        writer.Key("openPaths");
        writer.StartArray();
        for (const std::string& path : pe.data.openPaths)
            writer.String(path.c_str());
        writer.EndArray();
        writer.EndObject();
    }
    writer.EndArray();

    ax::UserDefault* defaults = ax::UserDefault::getInstance();
    defaults->setStringForKey(kAssetBrowserStateKey, buffer.GetString());
    defaults->flush();
}

bool EditorPreferencesService::showRootBounds() const
{
    return preferences().contentView.showRootBounds;
}

void EditorPreferencesService::setShowRootBounds(bool value)
{
    EditorPreferences next = preferences();
    next.contentView.showRootBounds = value;
    savePreferences(next);
}

SceneColor EditorPreferencesService::rootBoundsColor() const
{
    return preferences().contentView.rootBoundsColor;
}

void EditorPreferencesService::setRootBoundsColor(const SceneColor& value)
{
    EditorPreferences next = preferences();
    next.contentView.rootBoundsColor = value;
    savePreferences(next);
}

std::string EditorPreferencesService::editorFontPath() const
{
    return preferences().editor.fontPath;
}

void EditorPreferencesService::setEditorFontPath(const std::string& path)
{
    EditorPreferences next = preferences();
    next.editor.fontPath = normalizedPathString(path);
    savePreferences(next);
}

void EditorPreferencesService::clearEditorFontPath()
{
    EditorPreferences next = preferences();
    ensureDefaultEditorFontExists();
    next.editor.fontPath = kDefaultEditorFontPath;
    savePreferences(next);
}

float EditorPreferencesService::editorFontSize() const
{
    return preferences().editor.fontSize;
}

void EditorPreferencesService::setEditorFontSize(float size)
{
    EditorPreferences next = preferences();
    next.editor.fontSize = size;
    savePreferences(next);
}

void EditorPreferencesService::clearEditorFontSize()
{
    EditorPreferences next = preferences();
    next.editor.fontSize = defaultEditorPreferences().editor.fontSize;
    savePreferences(next);
}

SceneColor EditorPreferencesService::objectNoteColor() const
{
    return preferences().sceneDefaults.objectNote.color;
}

void EditorPreferencesService::setObjectNoteColor(const SceneColor& value)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.color = value;
    savePreferences(next);
}

void EditorPreferencesService::clearObjectNoteColor()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.color = defaultEditorPreferences().sceneDefaults.objectNote.color;
    savePreferences(next);
}

float EditorPreferencesService::objectNoteFontSize() const
{
    return preferences().sceneDefaults.objectNote.fontSize;
}

void EditorPreferencesService::setObjectNoteFontSize(float size)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.fontSize = size;
    savePreferences(next);
}

void EditorPreferencesService::clearObjectNoteFontSize()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.fontSize = defaultEditorPreferences().sceneDefaults.objectNote.fontSize;
    savePreferences(next);
}

bool EditorPreferencesService::objectNoteOutlineEnabled() const
{
    return preferences().sceneDefaults.objectNote.outlineEnabled;
}

void EditorPreferencesService::setObjectNoteOutlineEnabled(bool value)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.outlineEnabled = value;
    savePreferences(next);
}

void EditorPreferencesService::clearObjectNoteOutlineEnabled()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.outlineEnabled = defaultEditorPreferences().sceneDefaults.objectNote.outlineEnabled;
    savePreferences(next);
}

SceneColor EditorPreferencesService::objectNoteOutlineColor() const
{
    return preferences().sceneDefaults.objectNote.outlineColor;
}

void EditorPreferencesService::setObjectNoteOutlineColor(const SceneColor& value)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.outlineColor = value;
    savePreferences(next);
}

void EditorPreferencesService::clearObjectNoteOutlineColor()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.outlineColor = defaultEditorPreferences().sceneDefaults.objectNote.outlineColor;
    savePreferences(next);
}

float EditorPreferencesService::objectNoteOutlineSize() const
{
    return preferences().sceneDefaults.objectNote.outlineSize;
}

void EditorPreferencesService::setObjectNoteOutlineSize(float size)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.outlineSize = size;
    savePreferences(next);
}

void EditorPreferencesService::clearObjectNoteOutlineSize()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.objectNote.outlineSize = defaultEditorPreferences().sceneDefaults.objectNote.outlineSize;
    savePreferences(next);
}

SceneColor EditorPreferencesService::shapeFillColor() const
{
    return preferences().sceneDefaults.shape.fillColor;
}

void EditorPreferencesService::setShapeFillColor(const SceneColor& value)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.shape.fillColor = value;
    savePreferences(next);
}

void EditorPreferencesService::clearShapeFillColor()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.shape.fillColor = defaultEditorPreferences().sceneDefaults.shape.fillColor;
    savePreferences(next);
}

SceneColor EditorPreferencesService::shapeStrokeColor() const
{
    return preferences().sceneDefaults.shape.strokeColor;
}

void EditorPreferencesService::setShapeStrokeColor(const SceneColor& value)
{
    EditorPreferences next = preferences();
    next.sceneDefaults.shape.strokeColor = value;
    savePreferences(next);
}

void EditorPreferencesService::clearShapeStrokeColor()
{
    EditorPreferences next = preferences();
    next.sceneDefaults.shape.strokeColor = defaultEditorPreferences().sceneDefaults.shape.strokeColor;
    savePreferences(next);
}

std::string EditorPreferencesService::workingDirectoryOverride() const
{
    return preferences().workspace.workingDirectoryOverride;
}

void EditorPreferencesService::setWorkingDirectoryOverride(const std::string& path)
{
    EditorPreferences next = preferences();
    next.workspace.workingDirectoryOverride = normalizedPathString(path);
    savePreferences(next);
}

void EditorPreferencesService::clearWorkingDirectoryOverride()
{
    EditorPreferences next = preferences();
    next.workspace.workingDirectoryOverride = defaultEditorPreferences().workspace.workingDirectoryOverride;
    savePreferences(next);
}

void EditorPreferencesService::applyWorkingDirectory(ProjectSettings& settings) const
{
    const std::string overridePath = workingDirectoryOverride();
    const std::string root = overridePath.empty() ? "Content" : overridePath;
    settings.setResourceRoot(root);
    settings.setConfigRoot(root);
    addSearchPathToFront(settings.resourceRoot());
}

std::string EditorPreferencesService::npkExporterDirectory() const
{
    return preferences().npkExporter.npkDirectory;
}

void EditorPreferencesService::setNpkExporterDirectory(const std::string& path)
{
    EditorPreferences next = preferences();
    next.npkExporter.npkDirectory = path;
    savePreferences(next);
}

std::string EditorPreferencesService::npkExporterOutputRoot() const
{
    return preferences().npkExporter.outputRoot;
}

void EditorPreferencesService::setNpkExporterOutputRoot(const std::string& path)
{
    EditorPreferences next = preferences();
    next.npkExporter.outputRoot = path;
    savePreferences(next);
}

std::string EditorPreferencesService::npkExporterFilter() const
{
    return preferences().npkExporter.imgFilter;
}

void EditorPreferencesService::setNpkExporterFilter(const std::string& filter)
{
    EditorPreferences next = preferences();
    next.npkExporter.imgFilter = filter;
    savePreferences(next);
}
}  // namespace editor
