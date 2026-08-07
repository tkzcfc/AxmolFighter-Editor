#include "documents/LayerDocument.h"

#include "core/EditorPreferencesService.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace editor
{
namespace
{
using JsonValue = rapidjson::Value;

float numberOr(const JsonValue& object, const char* key, float fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsNumber())
        return fallback;
    return object[key].GetFloat();
}

bool boolOr(const JsonValue& object, const char* key, bool fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsBool())
        return fallback;
    return object[key].GetBool();
}

int intOr(const JsonValue& object, const char* key, int fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsInt())
        return fallback;
    return object[key].GetInt();
}

std::string stringOr(const JsonValue& object, const char* key, const std::string& fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsString())
        return fallback;
    return object[key].GetString();
}

SceneVec2 vec2Or(const JsonValue& object, const char* key, SceneVec2 fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
        return fallback;

    const JsonValue& value = object[key];
    fallback.x = numberOr(value, "x", fallback.x);
    fallback.y = numberOr(value, "y", fallback.y);
    return fallback;
}

SceneVec2 vec2ValueOr(const JsonValue& value, SceneVec2 fallback)
{
    if (!value.IsObject())
        return fallback;
    fallback.x = numberOr(value, "x", fallback.x);
    fallback.y = numberOr(value, "y", fallback.y);
    return fallback;
}

SceneSize sizeOr(const JsonValue& object, const char* key, SceneSize fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
        return fallback;

    const JsonValue& value = object[key];
    fallback.width = numberOr(value, "width", fallback.width);
    fallback.height = numberOr(value, "height", fallback.height);
    return fallback;
}

SceneColor colorOr(const JsonValue& object, const char* key, SceneColor fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
        return fallback;

    const JsonValue& value = object[key];
    fallback.r = numberOr(value, "r", fallback.r);
    fallback.g = numberOr(value, "g", fallback.g);
    fallback.b = numberOr(value, "b", fallback.b);
    fallback.a = numberOr(value, "a", fallback.a);
    return fallback;
}

LayerShape defaultShape(const std::string& type)
{
    LayerShape shape;
    shape.type = type;
    shape.name = type;
    if (type == "Circle")
    {
        shape.radius = 50.0f;
        shape.segments = 32;
        shape.size = {100.0f, 100.0f};
    }
    else if (type == "Polygon")
    {
        shape.points = {{-50.0f, -40.0f}, {50.0f, -40.0f}, {0.0f, 50.0f}};
        shape.size = {100.0f, 100.0f};
    }
    else
    {
        shape.type = "Rectangle";
        shape.name = "Rectangle";
        shape.size = {100.0f, 100.0f};
    }
    return shape;
}

LayerShape parseShape(const JsonValue& value)
{
    LayerShape shape = defaultShape(stringOr(value, "type", "Rectangle"));
    if (!value.IsObject())
        return shape;

    shape.id = stringOr(value, "id", shape.id);
    shape.type = stringOr(value, "type", shape.type);
    shape.name = stringOr(value, "name", shape.name);
    shape.visible = boolOr(value, "visible", shape.visible);
    shape.locked = boolOr(value, "locked", shape.locked);
    shape.position = vec2Or(value, "position", shape.position);
    shape.rotation = numberOr(value, "rotation", shape.rotation);
    shape.color = colorOr(value, "color", shape.color);
    shape.strokeColor = colorOr(value, "strokeColor", shape.strokeColor);
    shape.opacity = std::clamp(intOr(value, "opacity", shape.opacity), 0, 255);
    shape.size = sizeOr(value, "size", shape.size);
    shape.radius = numberOr(value, "radius", shape.radius);
    shape.segments = std::clamp(intOr(value, "segments", shape.segments), 3, 256);
    if (value.HasMember("points") && value["points"].IsArray())
    {
        shape.points.clear();
        for (const JsonValue& pointValue : value["points"].GetArray())
            shape.points.push_back(vec2ValueOr(pointValue, {}));
    }
    if (shape.points.empty() && shape.type == "Polygon")
        shape.points = defaultShape("Polygon").points;

    shape.properties.clear();
    if (value.HasMember("properties") && value["properties"].IsArray())
    {
        for (const JsonValue& propertyValue : value["properties"].GetArray())
        {
            if (!propertyValue.IsObject())
                continue;

            ObjectKeyValue property;
            property.key = stringOr(propertyValue, "key", property.key);
            const std::string type = stringOr(propertyValue, "type", "String");
            if (type == "Int")
            {
                property.type = "Int";
                if (propertyValue.HasMember("value") && propertyValue["value"].IsNumber())
                    property.intValue = propertyValue["value"].GetInt();
            }
            else if (type == "Float")
            {
                property.type = "Float";
                if (propertyValue.HasMember("value") && propertyValue["value"].IsNumber())
                    property.floatValue = propertyValue["value"].GetFloat();
            }
            else
            {
                property.type = "String";
                property.stringValue = stringOr(propertyValue, "value", property.stringValue);
            }
            shape.properties.push_back(std::move(property));
        }
    }
    return shape;
}

void writeVec2(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const SceneVec2& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("x");
    writer.Double(value.x);
    writer.Key("y");
    writer.Double(value.y);
    writer.EndObject();
}

void writeSize(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const SceneSize& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("width");
    writer.Double(value.width);
    writer.Key("height");
    writer.Double(value.height);
    writer.EndObject();
}

void writeColor(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const SceneColor& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("r");
    writer.Double(value.r);
    writer.Key("g");
    writer.Double(value.g);
    writer.Key("b");
    writer.Double(value.b);
    writer.Key("a");
    writer.Double(value.a);
    writer.EndObject();
}

void writeVec2Value(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const SceneVec2& value)
{
    writer.StartObject();
    writer.Key("x");
    writer.Double(value.x);
    writer.Key("y");
    writer.Double(value.y);
    writer.EndObject();
}

void writeShape(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const LayerShape& shape)
{
    writer.StartObject();
    writer.Key("id");
    writer.String(shape.id.c_str());
    writer.Key("type");
    writer.String(shape.type.c_str());
    writer.Key("name");
    writer.String(shape.name.c_str());
    writer.Key("visible");
    writer.Bool(shape.visible);
    writer.Key("locked");
    writer.Bool(shape.locked);
    writeVec2(writer, "position", shape.position);
    writer.Key("rotation");
    writer.Double(shape.rotation);
    writeColor(writer, "color", shape.color);
    writeColor(writer, "strokeColor", shape.strokeColor);
    writer.Key("opacity");
    writer.Int(std::clamp(shape.opacity, 0, 255));
    writeSize(writer, "size", shape.size);
    writer.Key("radius");
    writer.Double(shape.radius);
    writer.Key("segments");
    writer.Int(std::clamp(shape.segments, 3, 256));
    writer.Key("points");
    writer.StartArray();
    for (const SceneVec2& point : shape.points)
        writeVec2Value(writer, point);
    writer.EndArray();
    writer.Key("properties");
    writer.StartArray();
    for (const ObjectKeyValue& property : shape.properties)
    {
        writer.StartObject();
        writer.Key("key");
        writer.String(property.key.c_str());
        writer.Key("type");
        writer.String(property.type.c_str());
        writer.Key("value");
        if (property.type == "Int")
            writer.Int(property.intValue);
        else if (property.type == "Float")
            writer.Double(property.floatValue);
        else
            writer.String(property.stringValue.c_str());
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
}

bool startsWithShapePrefix(const std::string& id)
{
    constexpr const char* kPrefix = "shape_";
    return id.rfind(kPrefix, 0) == 0;
}

int suffixedNameNumber(const std::string& name, const std::string& baseName)
{
    const std::string prefix = baseName + "_";
    if (name.rfind(prefix, 0) != 0)
        return 0;

    const std::string suffix = name.substr(prefix.size());
    if (suffix.empty() ||
        !std::all_of(suffix.begin(), suffix.end(), [](unsigned char value) { return std::isdigit(value); }))
    {
        return 0;
    }
    return std::stoi(suffix);
}

std::string shapePluginToJson(const std::string& selectedShapeId,
                              int nextShapeSerial,
                              const std::vector<LayerShape>& shapes)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("selectedShapeId");
    writer.String(selectedShapeId.c_str());
    writer.Key("nextShapeSerial");
    writer.Int(nextShapeSerial);
    writer.Key("shapes");
    writer.StartArray();
    for (const LayerShape& shape : shapes)
        writeShape(writer, shape);
    writer.EndArray();
    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}
}  // namespace

bool LayerDocument::open(const std::filesystem::path& path)
{
    m_lastError.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_lastError = "Failed to open layer file: " + path.generic_string();
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!loadFromJson(buffer.str()))
        return false;

    m_path = path.lexically_normal();
    setSceneDirty(false);
    resetSceneUndoHistory();
    return true;
}

bool LayerDocument::save()
{
    m_lastError.clear();
    if (m_path.empty())
    {
        m_lastError = "Layer document path is empty.";
        return false;
    }

    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        m_lastError = "Failed to open layer file for writing: " + m_path.generic_string();
        return false;
    }

    file << toJson();
    if (!file)
    {
        m_lastError = "Failed to write layer file: " + m_path.generic_string();
        return false;
    }

    markSceneSaved();
    return true;
}

bool LayerDocument::isDirty() const
{
    return isSceneDirty();
}

std::string LayerDocument::getDisplayName() const
{
    return m_path.empty() ? "Untitled.layer" : m_path.filename().generic_string();
}

const std::filesystem::path& LayerDocument::path() const
{
    return m_path;
}

std::string LayerDocument::lastError() const
{
    return m_lastError;
}

bool LayerDocument::createDefaultFile(const std::filesystem::path& path, std::string& message)
{
    message.clear();
    LayerDocument document;
    document.resetToDefault();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        message = "Failed to create directory: " + path.parent_path().generic_string();
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        message = "Failed to create layer file: " + path.generic_string();
        return false;
    }

    file << document.toJson();
    if (!file)
    {
        message = "Failed to write layer file: " + path.generic_string();
        return false;
    }
    return true;
}

bool LayerDocument::hasShapeSelection() const
{
    return !m_selectedShapeId.empty() && selectedShape();
}

const std::string& LayerDocument::selectedShapeId() const
{
    return m_selectedShapeId;
}

bool LayerDocument::selectShape(const std::string& id)
{
    const auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [&](const LayerShape& shape) {
        return shape.id == id;
    });
    if (it == m_shapes.end())
        return false;
    m_selectedShapeId = id;
    clearNodeSelection();
    return true;
}

void LayerDocument::clearShapeSelection()
{
    m_selectedShapeId.clear();
    if (selectedNodeId().empty())
        selectNode("root");
}

LayerShape* LayerDocument::selectedShape()
{
    auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [&](const LayerShape& shape) {
        return shape.id == m_selectedShapeId;
    });
    return it == m_shapes.end() ? nullptr : &*it;
}

const LayerShape* LayerDocument::selectedShape() const
{
    auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [&](const LayerShape& shape) {
        return shape.id == m_selectedShapeId;
    });
    return it == m_shapes.end() ? nullptr : &*it;
}

std::vector<LayerShape>& LayerDocument::shapes()
{
    return m_shapes;
}

const std::vector<LayerShape>& LayerDocument::shapes() const
{
    return m_shapes;
}

LayerShape* LayerDocument::addShape(const std::string& type)
{
    beginUndoTransaction();
    LayerShape shape = defaultShape(type);
    const EditorPreferencesService preferences;
    shape.color = preferences.shapeFillColor();
    shape.strokeColor = preferences.shapeStrokeColor();
    shape.id = nextShapeId();
    shape.name = nextUniqueShapeName(shape.type);
    m_shapes.push_back(std::move(shape));
    LayerShape* added = &m_shapes.back();
    m_selectedShapeId = added->id;
    clearNodeSelection();
    markDirty();
    commitUndoTransaction();
    return added;
}

bool LayerDocument::canDeleteShape(const std::string& id) const
{
    return std::any_of(m_shapes.begin(), m_shapes.end(), [&](const LayerShape& shape) {
        return shape.id == id;
    });
}

bool LayerDocument::deleteShape(const std::string& id)
{
    auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [&](const LayerShape& shape) {
        return shape.id == id;
    });
    if (it == m_shapes.end())
        return false;

    beginUndoTransaction();
    m_shapes.erase(it);
    if (m_selectedShapeId == id)
    {
        m_selectedShapeId.clear();
        selectNode("root");
    }
    markDirty();
    commitUndoTransaction();
    return true;
}

bool LayerDocument::loadFromJson(const std::string& json)
{
    rapidjson::Document document;
    document.Parse(json.c_str());
    if (document.HasParseError())
    {
        m_lastError = "Invalid layer JSON: ";
        m_lastError += rapidjson::GetParseError_En(document.GetParseError());
        m_lastError += " at offset " + std::to_string(document.GetErrorOffset()) + ".";
        return false;
    }

    if (!document.IsObject())
    {
        m_lastError = "Invalid layer JSON: root must be an object.";
        return false;
    }

    const std::string type = stringOr(document, "type", "layer");
    if (type != "layer")
    {
        m_lastError = "Invalid layer JSON: type must be \"layer\".";
        return false;
    }

    m_version = static_cast<int>(numberOr(document, "version", static_cast<float>(kCurrentVersion)));
    if (document.HasMember("root"))
        loadSceneRootFromJson(document["root"]);
    else
        resetSceneToDefault();

    m_selectedShapeId.clear();
    m_shapes.clear();
    m_nextShapeSerial = 1;

    if (document.HasMember("plugins") && document["plugins"].IsObject())
    {
        const JsonValue& plugins = document["plugins"];
        if (plugins.HasMember("shape") && plugins["shape"].IsObject())
        {
            const JsonValue& shapePlugin = plugins["shape"];
            m_selectedShapeId = stringOr(shapePlugin, "selectedShapeId", "");
            m_nextShapeSerial = std::max(1, intOr(shapePlugin, "nextShapeSerial", 1));
            if (shapePlugin.HasMember("shapes") && shapePlugin["shapes"].IsArray())
            {
                for (const JsonValue& shapeValue : shapePlugin["shapes"].GetArray())
                    m_shapes.push_back(parseShape(shapeValue));
            }
        }
    }
    scanShapeIds();
    if (!m_selectedShapeId.empty() && !selectedShape())
        m_selectedShapeId.clear();
    if (!m_selectedShapeId.empty())
        clearNodeSelection();
    return true;
}

std::string LayerDocument::toJson() const
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("type");
    writer.String("layer");
    writer.Key("version");
    writer.Int(kCurrentVersion);
    writer.Key("root");
    writeSceneRootJson(writer);
    writer.Key("plugins");
    writer.StartObject();
    writer.Key("shape");
    writer.StartObject();
    writer.Key("selectedShapeId");
    writer.String(m_selectedShapeId.c_str());
    writer.Key("nextShapeSerial");
    writer.Int(m_nextShapeSerial);
    writer.Key("shapes");
    writer.StartArray();
    for (const LayerShape& shape : m_shapes)
        writeShape(writer, shape);
    writer.EndArray();
    writer.EndObject();
    writer.EndObject();
    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

void LayerDocument::resetToDefault()
{
    resetSceneToDefault();
    m_selectedShapeId.clear();
    m_shapes.clear();
    m_version = kCurrentVersion;
    m_nextShapeSerial = 1;
    m_lastError.clear();
    resetSceneUndoHistory();
}

std::string LayerDocument::nextShapeId()
{
    std::string id;
    do
    {
        id = "shape_" + std::to_string(m_nextShapeSerial++);
    } while (std::any_of(m_shapes.begin(), m_shapes.end(), [&](const LayerShape& shape) { return shape.id == id; }));
    return id;
}

void LayerDocument::scanShapeIds()
{
    for (const LayerShape& shape : m_shapes)
    {
        if (startsWithShapePrefix(shape.id))
        {
            const std::string suffix = shape.id.substr(6);
            if (!suffix.empty() &&
                std::all_of(suffix.begin(), suffix.end(), [](unsigned char value) { return std::isdigit(value); }))
            {
                m_nextShapeSerial = std::max(m_nextShapeSerial, std::stoi(suffix) + 1);
            }
        }
    }
}

std::string LayerDocument::nextUniqueShapeName(const std::string& baseName) const
{
    int maxNumber = 0;
    for (const LayerShape& shape : m_shapes)
        maxNumber = std::max(maxNumber, suffixedNameNumber(shape.name, baseName));
    return baseName + "_" + std::to_string(maxNumber + 1);
}

void LayerDocument::onSceneNodeSelected()
{
    m_selectedShapeId.clear();
}

void LayerDocument::onSceneNodeSelectionCleared() {}

std::string LayerDocument::captureSceneExtensionJson() const
{
    return shapePluginToJson(m_selectedShapeId, m_nextShapeSerial, m_shapes);
}

void LayerDocument::restoreSceneExtensionJson(const std::string& json)
{
    m_selectedShapeId.clear();
    m_shapes.clear();
    m_nextShapeSerial = 1;

    rapidjson::Document document;
    document.Parse(json.c_str());
    if (!document.IsObject())
        return;

    m_selectedShapeId = stringOr(document, "selectedShapeId", "");
    m_nextShapeSerial = std::max(1, intOr(document, "nextShapeSerial", 1));
    if (document.HasMember("shapes") && document["shapes"].IsArray())
    {
        for (const JsonValue& shapeValue : document["shapes"].GetArray())
            m_shapes.push_back(parseShape(shapeValue));
    }
    scanShapeIds();
    if (!m_selectedShapeId.empty() && !selectedShape())
        m_selectedShapeId.clear();
    if (!m_selectedShapeId.empty())
        clearNodeSelection();
}

void LayerDocument::writeSceneExtensionSnapshot(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                                const std::string& extensionJson) const
{
    writer.Key("shape");
    writer.String(extensionJson.c_str());
}

std::string LayerDocument::sceneDirtyExtensionJson(const std::string& extensionJson) const
{
    rapidjson::Document document;
    document.Parse(extensionJson.c_str());
    if (document.HasParseError() || !document.IsObject())
        return extensionJson;

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("nextShapeSerial");
    writer.Int(intOr(document, "nextShapeSerial", 1));
    writer.Key("shapes");
    if (document.HasMember("shapes") && document["shapes"].IsArray())
    {
        document["shapes"].Accept(writer);
    }
    else
    {
        writer.StartArray();
        writer.EndArray();
    }
    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}
}  // namespace editor
