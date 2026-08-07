#include "scene/SceneDocument.h"

#include "scene/node_editors/SceneNodeEditor.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <sstream>

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

SceneNode defaultRootNode()
{
    SceneNode root;
    root.id = "root";
    root.name = "Root";
    root.size = {960.0f, 640.0f};
    return root;
}

SceneNode parseNode(const JsonValue& value, const SceneNode& fallback)
{
    SceneNode node = fallback;
    if (!value.IsObject())
        return node;

    node.id = stringOr(value, "id", node.id);
    node.type = stringOr(value, "type", node.type);
    node.name = stringOr(value, "name", node.name);
    node.note = stringOr(value, "note", node.note);
    node.visible = boolOr(value, "visible", node.visible);
    node.locked = boolOr(value, "locked", node.locked);
    node.position = vec2Or(value, "position", node.position);
    node.positionZ = numberOr(value, "positionZ", node.positionZ);
    node.size = sizeOr(value, "size", node.size);
    node.anchor = vec2Or(value, "anchor", node.anchor);
    node.scale = vec2Or(value, "scale", node.scale);
    node.rotation = numberOr(value, "rotation", node.rotation);
    node.skew = vec2Or(value, "skew", node.skew);
    node.color = colorOr(value, "color", node.color);
    node.opacity = std::clamp(intOr(value, "opacity", node.opacity), 0, 255);
    if (const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(node.type))
        editor->readCustomData(node, value);
    node.children.clear();

    if (value.HasMember("children") && value["children"].IsArray())
    {
        int childIndex = 1;
        for (const JsonValue& childValue : value["children"].GetArray())
        {
            SceneNode child;
            child.id = "node_" + std::to_string(childIndex++);
            child.name = "Node";
            node.children.push_back(parseNode(childValue, child));
        }
    }
    return node;
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

void writeNode(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const SceneNode& node)
{
    writer.StartObject();
    writer.Key("id");
    writer.String(node.id.c_str());
    writer.Key("type");
    writer.String(node.type.c_str());
    writer.Key("name");
    writer.String(node.name.c_str());
    writer.Key("note");
    writer.String(node.note.c_str());
    writer.Key("visible");
    writer.Bool(node.visible);
    writer.Key("locked");
    writer.Bool(node.locked);
    writeVec2(writer, "position", node.position);
    writer.Key("positionZ");
    writer.Double(node.positionZ);
    writeSize(writer, "size", node.size);
    writeVec2(writer, "anchor", node.anchor);
    writeVec2(writer, "scale", node.scale);
    writer.Key("rotation");
    writer.Double(node.rotation);
    writeVec2(writer, "skew", node.skew);
    writeColor(writer, "color", node.color);
    writer.Key("opacity");
    writer.Int(std::clamp(node.opacity, 0, 255));
    if (const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(node.type))
        editor->writeCustomData(writer, node);
    writer.Key("children");
    writer.StartArray();
    for (const SceneNode& child : node.children)
        writeNode(writer, child);
    writer.EndArray();
    writer.EndObject();
}

int countNodes(const SceneNode& node)
{
    int total = 1;
    for (const SceneNode& child : node.children)
        total += countNodes(child);
    return total;
}

bool startsWithNodePrefix(const std::string& id)
{
    constexpr const char* kPrefix = "node_";
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

int maxNodeNameNumber(const SceneNode& node, const std::string& baseName)
{
    int result = suffixedNameNumber(node.name, baseName);
    for (const SceneNode& child : node.children)
        result = std::max(result, maxNodeNameNumber(child, baseName));
    return result;
}
}  // namespace

SceneNode& SceneDocument::root()
{
    return m_root;
}

const SceneNode& SceneDocument::root() const
{
    return m_root;
}

SceneNode* SceneDocument::selectedNode()
{
    return findNode(m_selectedNodeId);
}

const SceneNode* SceneDocument::selectedNode() const
{
    return findNode(m_selectedNodeId);
}

const std::string& SceneDocument::selectedNodeId() const
{
    return m_selectedNodeId;
}

bool SceneDocument::selectNode(const std::string& id)
{
    if (!findNode(id))
        return false;
    m_selectedNodeId = id;
    onSceneNodeSelected();
    return true;
}

void SceneDocument::clearNodeSelection()
{
    m_selectedNodeId.clear();
    onSceneNodeSelectionCleared();
}

SceneNode* SceneDocument::addChildOfType(const std::string& parentId, const std::string& typeId)
{
    SceneNode* parent = findNode(parentId);
    if (!parent)
        return nullptr;

    if (!canAddChild(parentId))
        return nullptr;

    const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(typeId);
    if (!editor)
        return nullptr;

    beginUndoTransaction();
    SceneNode child;
    child.id = nextNodeId();
    editor->initializeNode(child);
    child.name = nextUniqueNodeName(std::string(editor->defaultName()));
    parent->children.push_back(std::move(child));
    SceneNode* added = &parent->children.back();
    m_selectedNodeId = added->id;
    onSceneNodeSelected();
    markDirty();
    commitUndoTransaction();
    return added;
}

SceneNode* SceneDocument::addChildNode(const std::string& parentId)
{
    return addChildOfType(parentId, "Node");
}

SceneNode* SceneDocument::addChildSprite(const std::string& parentId)
{
    return addChildOfType(parentId, "Sprite");
}

bool SceneDocument::canAddChild(const std::string& parentId) const
{
    const SceneNode* parent = findNode(parentId);
    if (!parent)
        return false;

    const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(parent->type);
    return !editor || editor->canHaveChildren();
}

bool SceneDocument::canReparent(const std::string& nodeId, const std::string& newParentId) const
{
    if (nodeId.empty() || nodeId == "root" || nodeId == newParentId)
        return false;

    const SceneNode* node = findNode(nodeId);
    const SceneNode* newParent = findNode(newParentId);
    if (!node || !newParent)
        return false;

    if (!canAddChild(newParentId))
        return false;

    return !containsNode(*node, newParentId);
}

bool SceneDocument::reparentNode(const std::string& nodeId, const std::string& newParentId)
{
    if (!canReparent(nodeId, newParentId))
        return false;

    beginUndoTransaction();
    std::optional<SceneNode> detached = detachNode(nodeId);
    SceneNode* newParent = findNode(newParentId);
    if (!detached || !newParent)
    {
        commitUndoTransaction();
        return false;
    }

    newParent->children.push_back(std::move(*detached));
    m_selectedNodeId = nodeId;
    onSceneNodeSelected();
    markDirty();
    commitUndoTransaction();
    return true;
}

bool SceneDocument::canMoveNodeSiblingOrder(const std::string& nodeId, SceneNodeSiblingMove move) const
{
    if (nodeId.empty() || nodeId == "root")
        return false;

    const SceneNode* parent = findParent(nodeId);
    if (!parent || parent->children.size() < 2)
        return false;

    auto it = std::find_if(parent->children.begin(), parent->children.end(), [&](const SceneNode& child) {
        return child.id == nodeId;
    });
    if (it == parent->children.end())
        return false;

    const std::size_t index = static_cast<std::size_t>(std::distance(parent->children.begin(), it));
    switch (move)
    {
    case SceneNodeSiblingMove::Up:
    case SceneNodeSiblingMove::Top:
        return index + 1 < parent->children.size();
    case SceneNodeSiblingMove::Down:
    case SceneNodeSiblingMove::Bottom:
        return index > 0;
    }
    return false;
}

bool SceneDocument::moveNodeSiblingOrder(const std::string& nodeId, SceneNodeSiblingMove move)
{
    if (!canMoveNodeSiblingOrder(nodeId, move))
        return false;

    SceneNode* parent = findParent(nodeId);
    if (!parent)
        return false;

    auto it = std::find_if(parent->children.begin(), parent->children.end(), [&](const SceneNode& child) {
        return child.id == nodeId;
    });
    if (it == parent->children.end())
        return false;

    const std::size_t index = static_cast<std::size_t>(std::distance(parent->children.begin(), it));
    std::size_t targetIndex = index;
    switch (move)
    {
    case SceneNodeSiblingMove::Up:
        targetIndex = index + 1;
        break;
    case SceneNodeSiblingMove::Down:
        targetIndex = index - 1;
        break;
    case SceneNodeSiblingMove::Top:
        targetIndex = parent->children.size() - 1;
        break;
    case SceneNodeSiblingMove::Bottom:
        targetIndex = 0;
        break;
    }

    beginUndoTransaction();
    SceneNode moved = std::move(*it);
    parent->children.erase(it);
    parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(targetIndex), std::move(moved));
    m_selectedNodeId = nodeId;
    onSceneNodeSelected();
    markDirty();
    commitUndoTransaction();
    return true;
}

bool SceneDocument::canAlignSelectedNodeToParent() const
{
    const SceneNode* node = selectedNode();
    return node && node->id != "root" && !node->locked && findParent(node->id);
}

bool SceneDocument::alignSelectedNodeToParent(SceneNodeParentAlignment alignment)
{
    SceneNode* node = selectedNode();
    if (!node || node->id == "root" || node->locked)
        return false;

    const SceneNode* parent = findParent(node->id);
    if (!parent)
        return false;

    const float parentWidth = std::max(0.0f, parent->size.width);
    const float parentHeight = std::max(0.0f, parent->size.height);
    const float nodeWidth = std::max(0.0f, node->size.width);
    const float nodeHeight = std::max(0.0f, node->size.height);

    SceneVec2 target = node->position;
    switch (alignment)
    {
    case SceneNodeParentAlignment::Left:
        target.x = node->anchor.x * nodeWidth;
        break;
    case SceneNodeParentAlignment::HorizontalCenter:
        target.x = parentWidth * 0.5f + (node->anchor.x - 0.5f) * nodeWidth;
        break;
    case SceneNodeParentAlignment::Right:
        target.x = parentWidth - nodeWidth + node->anchor.x * nodeWidth;
        break;
    case SceneNodeParentAlignment::Top:
        target.y = parentHeight - nodeHeight + node->anchor.y * nodeHeight;
        break;
    case SceneNodeParentAlignment::VerticalCenter:
        target.y = parentHeight * 0.5f + (node->anchor.y - 0.5f) * nodeHeight;
        break;
    case SceneNodeParentAlignment::Bottom:
        target.y = node->anchor.y * nodeHeight;
        break;
    }

    if (std::abs(node->position.x - target.x) <= 0.0001f && std::abs(node->position.y - target.y) <= 0.0001f)
        return false;

    beginUndoTransaction();
    node->position = target;
    markDirty();
    commitUndoTransaction();
    return true;
}

bool SceneDocument::canDeleteNode(const std::string& nodeId) const
{
    if (nodeId.empty() || nodeId == "root")
        return false;

    const SceneNode* node = findNode(nodeId);
    return node && findParent(nodeId);
}

bool SceneDocument::deleteNode(const std::string& nodeId)
{
    if (!canDeleteNode(nodeId))
        return false;

    SceneNode* parent = findParent(nodeId);
    if (!parent)
        return false;

    beginUndoTransaction();
    auto it = std::find_if(parent->children.begin(), parent->children.end(), [&](const SceneNode& child) {
        return child.id == nodeId;
    });
    if (it == parent->children.end())
    {
        commitUndoTransaction();
        return false;
    }

    const std::string parentId = parent->id;
    parent->children.erase(it);
    m_selectedNodeId = parentId.empty() ? "root" : parentId;
    onSceneNodeSelected();
    markDirty();
    commitUndoTransaction();
    return true;
}

bool SceneDocument::copyNode(const std::string& nodeId, SceneNodeCopyData& outCopy) const
{
    if (nodeId.empty() || nodeId == "root")
        return false;

    const SceneNode* node = findNode(nodeId);
    if (!node)
        return false;

    const SceneNode* parent = findParent(nodeId);
    if (!parent)
        return false;

    outCopy.node = *node;
    outCopy.parentNodeId = parent->id.empty() ? "root" : parent->id;
    return true;
}

bool SceneDocument::copySelectedNode(SceneNodeCopyData& outCopy) const
{
    return copyNode(selectedNodeId(), outCopy);
}

SceneNode* SceneDocument::pasteNodeCopy(const SceneNodeCopyData& copy,
                                        const std::string& preferredParentId,
                                        const SceneVec2& rootPositionOffset)
{
    if (copy.node.id.empty() || copy.node.id == "root")
        return nullptr;

    std::string targetParentId = preferredParentId.empty() ? "root" : preferredParentId;
    if (!findNode(targetParentId) || !canAddChild(targetParentId))
        targetParentId = "root";
    if (!findNode(targetParentId) || !canAddChild(targetParentId))
        return nullptr;

    SceneNode* parent = findNode(targetParentId);
    if (!parent)
        return nullptr;

    beginUndoTransaction();
    SceneNode pasted = copy.node;
    assignFreshNodeIds(pasted);
    pasted.position.x += rootPositionOffset.x;
    pasted.position.y += rootPositionOffset.y;
    parent->children.push_back(std::move(pasted));
    SceneNode* added = &parent->children.back();
    m_selectedNodeId = added->id;
    onSceneNodeSelected();
    markDirty();
    commitUndoTransaction();
    return added;
}

int SceneDocument::nodeCount() const
{
    return countNodes(m_root);
}

bool SceneDocument::canUndo() const
{
    if (!m_undoStack.empty())
        return true;
    return m_undoTransactionActive && sceneSnapshotKey(captureSceneSnapshot()) != m_undoTransactionKey;
}

bool SceneDocument::undo()
{
    if (m_undoTransactionActive)
        commitUndoTransaction();

    if (m_undoStack.empty())
        return false;

    m_undoTransactionActive = false;
    const SceneSnapshot snapshot = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    restoreSceneSnapshot(snapshot);
    m_undoCheckpoint = captureSceneSnapshot();
    m_undoCheckpointKey = sceneSnapshotKey(m_undoCheckpoint);
    m_dirty = sceneDirtyKey(m_undoCheckpoint) != m_savedStateKey;
    return true;
}

bool SceneDocument::beginUndoTransaction()
{
    if (m_undoTransactionActive)
        return false;

    m_undoTransactionSnapshot = captureSceneSnapshot();
    m_undoTransactionKey = sceneSnapshotKey(m_undoTransactionSnapshot);
    m_undoTransactionActive = true;
    return true;
}

void SceneDocument::commitUndoTransaction()
{
    if (!m_undoTransactionActive)
    {
        markDirty();
        return;
    }

    const SceneSnapshot current = captureSceneSnapshot();
    const std::string currentKey = sceneSnapshotKey(current);
    if (currentKey != m_undoTransactionKey)
    {
        constexpr std::size_t kMaxUndoSnapshots = 128;
        m_undoStack.push_back(m_undoTransactionSnapshot);
        if (m_undoStack.size() > kMaxUndoSnapshots)
            m_undoStack.erase(m_undoStack.begin());
    }

    m_undoCheckpoint = current;
    m_undoCheckpointKey = currentKey;
    m_undoTransactionActive = false;
    m_undoTransactionKey.clear();
    m_dirty = sceneDirtyKey(current) != m_savedStateKey;
}

void SceneDocument::cancelUndoTransaction()
{
    m_undoTransactionActive = false;
    m_undoTransactionKey.clear();
}

bool SceneDocument::hasActiveUndoTransaction() const
{
    return m_undoTransactionActive;
}

void SceneDocument::markDirty()
{
    const SceneSnapshot current = captureSceneSnapshot();
    const std::string currentKey = sceneSnapshotKey(current);
    m_undoCheckpoint = current;
    m_undoCheckpointKey = currentKey;
    m_dirty = sceneDirtyKey(current) != m_savedStateKey;
}

void SceneDocument::resetSceneToDefault()
{
    m_root = defaultRootNode();
    m_selectedNodeId = "root";
    m_nextNodeSerial = 1;
    m_dirty = false;
    resetSceneUndoHistory();
}

void SceneDocument::resetSceneUndoHistory()
{
    m_undoStack.clear();
    m_undoCheckpoint = captureSceneSnapshot();
    m_undoCheckpointKey = sceneSnapshotKey(m_undoCheckpoint);
    m_savedStateKey = sceneDirtyKey(m_undoCheckpoint);
    m_undoTransactionActive = false;
    m_undoTransactionKey.clear();
}

void SceneDocument::markSceneSaved()
{
    m_dirty = false;
    m_undoCheckpoint = captureSceneSnapshot();
    m_savedStateKey = sceneDirtyKey(m_undoCheckpoint);
    m_undoCheckpointKey = sceneSnapshotKey(m_undoCheckpoint);
}

bool SceneDocument::isSceneDirty() const
{
    return m_dirty;
}

void SceneDocument::setSceneDirty(bool dirty)
{
    m_dirty = dirty;
}

bool SceneDocument::loadSceneRootFromJson(const rapidjson::Value& value)
{
    m_root = parseNode(value, defaultRootNode());
    if (m_root.id.empty())
        m_root.id = "root";
    m_root.id = "root";
    m_selectedNodeId = "root";
    m_nextNodeSerial = 1;
    scanNodeIds(m_root);
    return true;
}

void SceneDocument::writeSceneRootJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const
{
    writeNode(writer, m_root);
}

void SceneDocument::scanSceneNodeIds()
{
    m_nextNodeSerial = 1;
    scanNodeIds(m_root);
}

SceneNode* SceneDocument::findNode(const std::string& id)
{
    const auto find = [&](auto&& self, SceneNode& node) -> SceneNode* {
        if (node.id == id)
            return &node;
        for (SceneNode& child : node.children)
        {
            if (SceneNode* result = self(self, child))
                return result;
        }
        return nullptr;
    };
    return find(find, m_root);
}

const SceneNode* SceneDocument::findNode(const std::string& id) const
{
    const auto find = [&](auto&& self, const SceneNode& node) -> const SceneNode* {
        if (node.id == id)
            return &node;
        for (const SceneNode& child : node.children)
        {
            if (const SceneNode* result = self(self, child))
                return result;
        }
        return nullptr;
    };
    return find(find, m_root);
}

SceneNode* SceneDocument::findParent(const std::string& id)
{
    const auto find = [&](auto&& self, SceneNode& node) -> SceneNode* {
        for (SceneNode& child : node.children)
        {
            if (child.id == id)
                return &node;
            if (SceneNode* result = self(self, child))
                return result;
        }
        return nullptr;
    };
    return find(find, m_root);
}

const SceneNode* SceneDocument::findParent(const std::string& id) const
{
    const auto find = [&](auto&& self, const SceneNode& node) -> const SceneNode* {
        for (const SceneNode& child : node.children)
        {
            if (child.id == id)
                return &node;
            if (const SceneNode* result = self(self, child))
                return result;
        }
        return nullptr;
    };
    return find(find, m_root);
}

void SceneDocument::onSceneNodeSelected() {}

void SceneDocument::onSceneNodeSelectionCleared() {}

std::string SceneDocument::captureSceneExtensionJson() const
{
    return {};
}

void SceneDocument::restoreSceneExtensionJson(const std::string&) {}

void SceneDocument::writeSceneExtensionSnapshot(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                                const std::string& extensionJson) const
{
    writer.Key("extension");
    writer.String(extensionJson.c_str());
}

std::string SceneDocument::sceneDirtyExtensionJson(const std::string& extensionJson) const
{
    return extensionJson;
}

SceneDocument::SceneSnapshot SceneDocument::captureSceneSnapshot() const
{
    SceneSnapshot snapshot;
    snapshot.root = m_root;
    snapshot.selectedNodeId = m_selectedNodeId;
    snapshot.nextNodeSerial = m_nextNodeSerial;
    snapshot.extensionJson = captureSceneExtensionJson();
    return snapshot;
}

void SceneDocument::restoreSceneSnapshot(const SceneSnapshot& snapshot)
{
    m_root = snapshot.root;
    m_selectedNodeId = snapshot.selectedNodeId;
    m_nextNodeSerial = snapshot.nextNodeSerial;
    restoreSceneExtensionJson(snapshot.extensionJson);
    if (!m_selectedNodeId.empty() && !findNode(m_selectedNodeId))
        m_selectedNodeId = "root";
}

std::string SceneDocument::sceneSnapshotKey(const SceneSnapshot& snapshot) const
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("root");
    writeNode(writer, snapshot.root);
    writer.Key("selectedNodeId");
    writer.String(snapshot.selectedNodeId.c_str());
    writer.Key("nextNodeSerial");
    writer.Int(snapshot.nextNodeSerial);
    writeSceneExtensionSnapshot(writer, snapshot.extensionJson);
    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string SceneDocument::sceneDirtyKey(const SceneSnapshot& snapshot) const
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("root");
    writeNode(writer, snapshot.root);
    writeSceneExtensionSnapshot(writer, sceneDirtyExtensionJson(snapshot.extensionJson));
    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize());
}

bool SceneDocument::containsNode(const SceneNode& node, const std::string& id) const
{
    if (node.id == id)
        return true;
    for (const SceneNode& child : node.children)
    {
        if (containsNode(child, id))
            return true;
    }
    return false;
}

std::optional<SceneNode> SceneDocument::detachNode(const std::string& id)
{
    SceneNode* parent = findParent(id);
    if (!parent)
        return std::nullopt;

    auto it = std::find_if(parent->children.begin(), parent->children.end(), [&](const SceneNode& child) {
        return child.id == id;
    });
    if (it == parent->children.end())
        return std::nullopt;

    SceneNode detached = std::move(*it);
    parent->children.erase(it);
    return detached;
}

void SceneDocument::assignFreshNodeIds(SceneNode& node)
{
    node.id = nextNodeId();
    for (SceneNode& child : node.children)
        assignFreshNodeIds(child);
}

std::string SceneDocument::nextNodeId()
{
    std::string id;
    do
    {
        id = "node_" + std::to_string(m_nextNodeSerial++);
    } while (findNode(id));
    return id;
}

void SceneDocument::scanNodeIds(const SceneNode& node)
{
    if (startsWithNodePrefix(node.id))
    {
        const std::string suffix = node.id.substr(5);
        if (!suffix.empty() &&
            std::all_of(suffix.begin(), suffix.end(), [](unsigned char value) { return std::isdigit(value); }))
        {
            m_nextNodeSerial = std::max(m_nextNodeSerial, std::stoi(suffix) + 1);
        }
    }

    for (const SceneNode& child : node.children)
        scanNodeIds(child);
}

std::string SceneDocument::nextUniqueNodeName(const std::string& baseName) const
{
    return baseName + "_" + std::to_string(maxNodeNameNumber(m_root, baseName) + 1);
}
}  // namespace editor
