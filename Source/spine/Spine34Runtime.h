#pragma once

#include "2d/Node.h"
#include "base/Data.h"

#include <string>
#include <tuple>
#include <vector>

namespace editor
{
std::tuple<std::string, ax::Node*> createSpine34Animation(const ax::Data& skelData,
                                                          const std::string& jsonPath,
                                                          const std::string& atlasPath,
                                                          float scale);

bool spine34IsNode(ax::Node* node);
bool spine34IsValid(ax::Node* node);
void spine34CollectAnimations(ax::Node* node, std::vector<std::string>& out);
void spine34CollectSkins(ax::Node* node, std::vector<std::string>& out);
bool spine34HasAnimation(ax::Node* node, const std::string& name);
bool spine34HasSkin(ax::Node* node, const std::string& name);
float spine34AnimationDuration(ax::Node* node, const std::string& name);
bool spine34SetAnimation(ax::Node* node, int trackIndex, const std::string& name, bool loop);
void spine34KeepCurrentTrackAlive(ax::Node* node, int trackIndex);
void spine34SeekCurrentTrack(ax::Node* node, int trackIndex, float timeSeconds);
void spine34SetSkin(ax::Node* node, const std::string& name);
void spine34ClearTracks(ax::Node* node);
void spine34SetTimeScale(ax::Node* node, float scale);
void spine34SetDebugEnabled(ax::Node* node, bool bones, bool slots);
void spine34Update(ax::Node* node, float dt);
}  // namespace editor
