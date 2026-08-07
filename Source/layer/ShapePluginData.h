#pragma once

#include "scene/SceneTypes.h"

#include <string>
#include <vector>

namespace editor
{
struct LayerShape
{
    std::string id;
    std::string type = "Rectangle";
    std::string name = "Rectangle";
    bool visible = true;
    bool locked = false;
    SceneVec2 position;
    float rotation = 0.0f;
    SceneColor color;
    SceneColor strokeColor = {104.0f / 255.0f, 180.0f / 255.0f, 1.0f, 1.0f};
    int opacity = 255;
    SceneSize size = {100.0f, 100.0f};
    float radius = 50.0f;
    int segments = 32;
    std::vector<SceneVec2> points;
    std::vector<ObjectKeyValue> properties;
};
}  // namespace editor
