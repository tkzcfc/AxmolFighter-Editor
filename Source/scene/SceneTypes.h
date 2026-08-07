#pragma once

#include <string>
#include <map>
#include <vector>

namespace editor
{
struct SceneVec2
{
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const SceneVec2&) const = default;
};

struct SceneSize
{
    float width  = 0.0f;
    float height = 0.0f;

    bool operator==(const SceneSize&) const = default;
};

struct SceneColor
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    bool operator==(const SceneColor&) const = default;
};

struct SpriteNodeData
{
    std::string sourceType = "Texture";
    std::string imagePath;
    std::string atlasPath;
    std::string frameName;
    std::string renderType = "Simple";
    std::string blendSrc;
    std::string blendDst;
    SceneVec2 sliceCenterOrigin = {0.25f, 0.25f};
    SceneVec2 sliceCenterSize   = {0.5f, 0.5f};
};

struct LabelNodeData
{
    std::string text     = "Label";
    std::string fontType = "System";
    std::string fontName = "Arial";
    std::string fontPath;
    float fontSize                  = 24.0f;
    std::string autoSizeMode        = "None";
    std::string horizontalAlignment = "Center";
    std::string verticalAlignment   = "Center";
    SceneColor color;
    bool outlineEnabled = false;
    float outlineSize   = 1.0f;
    SceneColor outlineColor;
    bool shadowEnabled     = false;
    SceneVec2 shadowOffset = {2.0f, -2.0f};
    SceneColor shadowColor = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct SpineNodeData
{
    std::string jsonPath;
    std::string atlasPath;
    std::string animationName;
    std::string skinName;
    bool loop        = true;
    float timeScale  = 1.0f;
    bool debugBones  = false;
    bool debugSlots  = false;
    bool debugMeshes = false;
    bool autoSize    = false;
};

struct FrameAnimationNodeData
{
    std::string aniPath;
    std::string blendSrc;
    std::string blendDst;
    bool playing = true;
    bool loop    = true;
    std::map<std::string, std::string> previewVars;
};

struct ObjectKeyValue
{
    std::string key;
    std::string type = "String";
    int intValue     = 0;
    std::string stringValue;
    float floatValue = 0.0f;
};

struct ObjectNodeData
{
    std::string previewSourceType = "Texture";
    std::string previewImagePath;
    std::string previewAtlasPath;
    std::string previewFrameName;
    std::string note;
    SceneColor noteColor        = {0.26f, 0.59f, 0.98f, 1.0f};
    float noteFontSize          = 16.0f;
    bool noteOutlineEnabled     = true;
    SceneColor noteOutlineColor = {0.05f, 0.06f, 0.08f, 1.0f};
    float noteOutlineSize       = 2.0f;
    std::vector<ObjectKeyValue> properties;
};

struct NodeData
{
    bool drawFill = false;
};

struct SceneNode
{
    std::string id;
    std::string type = "Node";
    std::string name = "Node";
    // Editor-only annotation for hierarchy display; empty by default.
    std::string note;
    bool visible     = true;
    bool locked      = false;
    SceneVec2 position;
    float positionZ = 0.0f;
    SceneSize size;
    SceneVec2 anchor = {0.5f, 0.5f};
    SceneVec2 scale  = {1.0f, 1.0f};
    float rotation   = 0.0f;
    SceneVec2 skew;
    SceneColor color;
    int opacity = 255;
    NodeData node;
    SpriteNodeData sprite;
    LabelNodeData label;
    SpineNodeData spine;
    FrameAnimationNodeData frameAnimation;
    ObjectNodeData object;
    std::vector<SceneNode> children;
};

}  // namespace editor
