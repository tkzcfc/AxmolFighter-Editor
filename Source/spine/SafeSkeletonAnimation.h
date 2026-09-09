#pragma once

#include "2d/Node.h"

#include <string>
#include <tuple>
#include <vector>

namespace editor
{
// Loads Spine JSON or binary. Picks the 3.4 runtime when the file header contains "3.4.",
// otherwise uses engine 3.8. On success the returned node is autoreleased and owns
// skeletonData + atlas. On failure returns {error, nullptr}.
std::tuple<std::string, ax::Node*> createSafeSkeletonAnimation(
    const std::string& jsonPath,
    const std::string& atlasPath,
    float scale = 1.0f);

bool isEditorSpineNode(ax::Node* node);
void collectSpineAnimations(ax::Node* node, std::vector<std::string>& out);
void collectSpineSkins(ax::Node* node, std::vector<std::string>& out);
bool spineHasAnimation(ax::Node* node, const std::string& name);
bool spineHasSkin(ax::Node* node, const std::string& name);
float spineAnimationDuration(ax::Node* node, const std::string& name);
bool spineSetAnimation(ax::Node* node, int trackIndex, const std::string& name, bool loop);
void spineKeepCurrentTrackAlive(ax::Node* node, int trackIndex);
void spineSeekCurrentTrack(ax::Node* node, int trackIndex, float timeSeconds);
void spineSetSkin(ax::Node* node, const std::string& name);
void spineClearTracks(ax::Node* node);
void spineSetTimeScale(ax::Node* node, float scale);
void spineSetDebugEnabled(ax::Node* node, bool bones, bool slots, bool meshes);
void spineSetUpdateOnlyIfVisible(ax::Node* node, bool value);
void spineUpdate(ax::Node* node, float dt);
}  // namespace editor
