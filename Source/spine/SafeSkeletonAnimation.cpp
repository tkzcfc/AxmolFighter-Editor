#include "spine/SafeSkeletonAnimation.h"

#include "platform/FileUtils.h"
#include "spine/Animation.h"
#include "spine/AnimationState.h"
#include "spine/SkeletonAnimation.h"
#include "spine/SkeletonBinary.h"
#include "spine/SkeletonData.h"
#include "spine/SkeletonJson.h"
#include "spine/Skin.h"
#include "spine/Spine34Runtime.h"
#include "spine/spine-axmol.h"

#include <cstring>
#include <limits>
#include <new>
#include <string>

namespace editor
{
namespace
{
constexpr std::size_t kSpineVersionProbeBytes = 200;

class OwnedAtlasSkeletonAnimation : public spine::SkeletonAnimation
{
public:
    void takeAtlasOwnership(spine::Atlas* atlas, spine::AttachmentLoader* loader)
    {
        _atlas            = atlas;
        _attachmentLoader = loader;
        _ownsAtlas        = true;
    }
};

spine::AxmolTextureLoader& sharedTextureLoader()
{
    static spine::AxmolTextureLoader loader;
    return loader;
}

unsigned char firstPayloadByte(const ax::Data& data)
{
    const unsigned char* bytes = data.getBytes();
    size_t size                = static_cast<size_t>(data.getSize());
    size_t i                   = 0;
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        i = 3;
    while (i < size)
    {
        const unsigned char c = bytes[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            ++i;
            continue;
        }
        return c;
    }
    return 0;
}

bool headerLooksLikeSpine34(const ax::Data& data)
{
    const unsigned char* bytes = data.getBytes();
    const std::size_t size     = static_cast<std::size_t>(data.getSize());
    const std::size_t window   = size < kSpineVersionProbeBytes ? size : kSpineVersionProbeBytes;
    if (window < 3)
        return false;

    static constexpr char kMarker[] = "3.4";
    for (std::size_t i = 0; i + 3 <= window; ++i)
    {
        if (std::memcmp(bytes + i, kMarker, 3) == 0)
            return true;
    }
    return false;
}

std::tuple<std::string, ax::Node*> createSpine38Animation(const ax::Data& skelData,
                                                          const std::string& jsonPath,
                                                          const std::string& atlasPath,
                                                          float scale)
{
    spine::Atlas* atlas = new (__FILE__, __LINE__) spine::Atlas(atlasPath.c_str(), &sharedTextureLoader(), true);
    if (!atlas || atlas->getPages().size() == 0)
    {
        delete atlas;
        return {"Failed to read Spine atlas: " + atlasPath, nullptr};
    }

    auto* attachmentLoader            = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
    spine::SkeletonData* skeletonData = nullptr;
    const bool isJson                 = (firstPayloadByte(skelData) == static_cast<unsigned char>('{'));

    if (isJson)
    {
        std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                             static_cast<size_t>(skelData.getSize()));
        spine::SkeletonJson reader(attachmentLoader, false);
        reader.setScale(scale);
        skeletonData = reader.readSkeletonData(jsonText.c_str());
        if (!skeletonData)
        {
            const std::string error = !reader.getError().isEmpty() ? reader.getError().buffer()
                                                                   : ("Failed to read Spine JSON: " + jsonPath);
            delete attachmentLoader;
            delete atlas;
            return {error, nullptr};
        }
    }
    else
    {
        spine::SkeletonBinary reader(attachmentLoader, false);
        reader.setScale(scale);
        skeletonData = reader.readSkeletonData(skelData.getBytes(), static_cast<int>(skelData.getSize()));
        if (!skeletonData)
        {
            const std::string error = !reader.getError().isEmpty() ? reader.getError().buffer()
                                                                   : ("Failed to read Spine binary: " + jsonPath);
            delete attachmentLoader;
            delete atlas;
            return {error, nullptr};
        }
    }

    auto* animation = new (std::nothrow) OwnedAtlasSkeletonAnimation();
    if (!animation)
    {
        delete skeletonData;
        delete attachmentLoader;
        delete atlas;
        return {"Failed to allocate Spine node", nullptr};
    }

    animation->takeAtlasOwnership(atlas, attachmentLoader);
    animation->initWithData(skeletonData, true);
    animation->autorelease();
    return {"", animation};
}

spine::SkeletonAnimation* asSpine38(ax::Node* node)
{
    return dynamic_cast<spine::SkeletonAnimation*>(node);
}

bool spine38IsValid(ax::Node* node)
{
    auto* animation = asSpine38(node);
    return animation && animation->getSkeleton() && animation->getSkeleton()->getData();
}
}  // namespace

std::tuple<std::string, ax::Node*> createSafeSkeletonAnimation(
    const std::string& jsonPath,
    const std::string& atlasPath,
    float scale)
{
    if (jsonPath.empty())
        return {"Spine skeleton path is empty", nullptr};
    if (atlasPath.empty())
        return {"Spine atlas path is empty", nullptr};

    ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(jsonPath);
    if (skelData.isNull() || skelData.getSize() <= 0)
        return {"Failed to read Spine skeleton: " + jsonPath, nullptr};

    const bool prefer34 = headerLooksLikeSpine34(skelData);
    auto first          = prefer34 ? createSpine34Animation(skelData, jsonPath, atlasPath, scale)
                                   : createSpine38Animation(skelData, jsonPath, atlasPath, scale);
    if (std::get<1>(first))
        return first;

    auto second = prefer34 ? createSpine38Animation(skelData, jsonPath, atlasPath, scale)
                           : createSpine34Animation(skelData, jsonPath, atlasPath, scale);
    if (std::get<1>(second))
        return second;
    return first;
}

bool isEditorSpineNode(ax::Node* node)
{
    return spine38IsValid(node) || spine34IsValid(node);
}

void collectSpineAnimations(ax::Node* node, std::vector<std::string>& out)
{
    if (spine34IsNode(node))
    {
        spine34CollectAnimations(node, out);
        return;
    }
    auto* animation = asSpine38(node);
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->getData())
        return;
    spine::Vector<spine::Animation*>& animations = animation->getSkeleton()->getData()->getAnimations();
    for (size_t index = 0; index < animations.size(); ++index)
    {
        if (animations[index])
            out.emplace_back(animations[index]->getName().buffer());
    }
}

void collectSpineSkins(ax::Node* node, std::vector<std::string>& out)
{
    if (spine34IsNode(node))
    {
        spine34CollectSkins(node, out);
        return;
    }
    auto* animation = asSpine38(node);
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->getData())
        return;
    spine::Vector<spine::Skin*>& skins = animation->getSkeleton()->getData()->getSkins();
    for (size_t index = 0; index < skins.size(); ++index)
    {
        if (skins[index])
            out.emplace_back(skins[index]->getName().buffer());
    }
}

bool spineHasAnimation(ax::Node* node, const std::string& name)
{
    if (spine34IsNode(node))
        return spine34HasAnimation(node, name);
    auto* animation = asSpine38(node);
    if (!animation || name.empty())
        return false;
    return animation->findAnimation(name) != nullptr;
}

bool spineHasSkin(ax::Node* node, const std::string& name)
{
    if (spine34IsNode(node))
        return spine34HasSkin(node, name);
    auto* animation = asSpine38(node);
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->getData() || name.empty())
        return false;
    return animation->getSkeleton()->getData()->findSkin(name.c_str()) != nullptr;
}

float spineAnimationDuration(ax::Node* node, const std::string& name)
{
    if (spine34IsNode(node))
        return spine34AnimationDuration(node, name);
    auto* animation = asSpine38(node);
    if (!animation || name.empty())
        return 0.0f;
    spine::Animation* found = animation->findAnimation(name);
    return found ? found->getDuration() : 0.0f;
}

bool spineSetAnimation(ax::Node* node, int trackIndex, const std::string& name, bool loop)
{
    if (spine34IsNode(node))
        return spine34SetAnimation(node, trackIndex, name, loop);
    auto* animation = asSpine38(node);
    if (!animation || name.empty())
        return false;
    return animation->setAnimation(trackIndex, name, loop) != nullptr;
}

void spineKeepCurrentTrackAlive(ax::Node* node, int trackIndex)
{
    if (spine34IsNode(node))
    {
        spine34KeepCurrentTrackAlive(node, trackIndex);
        return;
    }
    auto* animation = asSpine38(node);
    if (!animation)
        return;
    if (spine::TrackEntry* entry = animation->getCurrent(trackIndex))
        entry->setTrackEnd(std::numeric_limits<float>::max());
}

void spineSeekCurrentTrack(ax::Node* node, int trackIndex, float timeSeconds)
{
    if (spine34IsNode(node))
    {
        spine34SeekCurrentTrack(node, trackIndex, timeSeconds);
        return;
    }
    auto* animation = asSpine38(node);
    if (!animation)
        return;
    if (spine::TrackEntry* entry = animation->getCurrent(trackIndex))
    {
        entry->setTrackTime(timeSeconds);
        entry->setAnimationLast(timeSeconds);
    }
}

void spineSetSkin(ax::Node* node, const std::string& name)
{
    if (spine34IsNode(node))
    {
        spine34SetSkin(node, name);
        return;
    }
    auto* animation = asSpine38(node);
    if (animation)
        animation->setSkin(name);
}

void spineClearTracks(ax::Node* node)
{
    if (spine34IsNode(node))
    {
        spine34ClearTracks(node);
        return;
    }
    auto* animation = asSpine38(node);
    if (animation)
        animation->clearTracks();
}

void spineSetTimeScale(ax::Node* node, float scale)
{
    if (spine34IsNode(node))
    {
        spine34SetTimeScale(node, scale);
        return;
    }
    auto* animation = asSpine38(node);
    if (animation)
        animation->setTimeScale(scale);
}

void spineSetDebugEnabled(ax::Node* node, bool bones, bool slots, bool meshes)
{
    if (spine34IsNode(node))
    {
        spine34SetDebugEnabled(node, bones, slots);
        return;
    }
    auto* animation = asSpine38(node);
    if (!animation)
        return;
    animation->setDebugBonesEnabled(bones);
    animation->setDebugSlotsEnabled(slots);
    animation->setDebugMeshesEnabled(meshes);
}

void spineSetUpdateOnlyIfVisible(ax::Node* node, bool value)
{
    auto* animation = asSpine38(node);
    if (animation)
        animation->setUpdateOnlyIfVisible(value);
}

void spineUpdate(ax::Node* node, float dt)
{
    if (spine34IsNode(node))
    {
        spine34Update(node, dt);
        return;
    }
    auto* animation = asSpine38(node);
    if (animation)
        animation->update(dt);
}
}  // namespace editor
