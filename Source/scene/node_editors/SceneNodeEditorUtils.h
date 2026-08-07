#pragma once

#include "asset_browser/AssetDragPayload.h"
#include "scene/SceneDocument.h"
#include "scene/node_editors/SceneNodeEditor.h"
#include "rapidjson/document.h"

#include "math/Color.h"

#include <functional>
#include <initializer_list>
#include <string>

namespace editor
{
std::string jsonStringOr(const rapidjson::Value& object, const char* key, const std::string& fallback);
float jsonNumberOr(const rapidjson::Value& object, const char* key, float fallback);
bool jsonBoolOr(const rapidjson::Value& object, const char* key, bool fallback);
SceneColor jsonColorOr(const rapidjson::Value& object, const char* key, SceneColor fallback);

bool editVec2(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, SceneVec2& value);
bool editSize(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, SceneSize& value);
bool editSizeWithPolicy(SceneDocument& document,
                        std::string& activeEditKey,
                        const std::string& key,
                        const char* label,
                        SceneSize& value,
                        const SceneNodeSizeEditPolicy& policy);
bool editFloat(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, float& value);
bool editIntRange(SceneDocument& document,
                  std::string& activeEditKey,
                  const std::string& key,
                  const char* label,
                  int& value,
                  int minValue,
                  int maxValue);
bool editBool(SceneDocument& document, const char* label, bool& value);
bool editString(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, std::string& value);
bool editMultilineString(SceneDocument& document,
                         std::string& activeEditKey,
                         const std::string& key,
                         const char* label,
                         std::string& value);
bool editColor(SceneDocument& document, std::string& activeEditKey, const std::string& key, const char* label, SceneColor& value);
bool editStringCombo(SceneDocument& document,
                     const char* label,
                     std::string& value,
                     const char* const* items,
                     int itemCount);

bool drawAssetReference(SceneDocument& document,
                        std::string& activeEditKey,
                        const std::string& key,
                        const char* label,
                        std::string& target,
                        std::initializer_list<AssetKind> allowedKinds,
                        const std::function<void(const AssetDragPayload&)>& assign);
}  // namespace editor
