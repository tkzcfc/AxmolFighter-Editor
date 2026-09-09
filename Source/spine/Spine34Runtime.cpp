#include "spine/Spine34Runtime.h"

#include "3rd/spine/spine_3_4/Cocos2dAttachmentLoader.h"
#include "3rd/spine/spine_3_4/extension.h"
#include "3rd/spine/spine_3_4/spine-cocos2dx.h"

#include <limits>
#include <new>
#include <string>

namespace editor
{
namespace
{
class OwnedAtlasSpine34Animation : public spine34::SkeletonAnimation
{
public:
    void takeAtlasOwnership(spAtlas* atlas, spAttachmentLoader* loader)
    {
        _atlas            = atlas;
        _attachmentLoader = loader;
    }
};

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

spine34::SkeletonAnimation* asSpine34(ax::Node* node)
{
    return dynamic_cast<spine34::SkeletonAnimation*>(node);
}
}  // namespace

std::tuple<std::string, ax::Node*> createSpine34Animation(const ax::Data& skelData,
                                                          const std::string& jsonPath,
                                                          const std::string& atlasPath,
                                                          float scale)
{
    spAtlas* atlas = spAtlas_createFromFile(atlasPath.c_str(), 0);
    if (!atlas || !atlas->pages)
    {
        if (atlas)
            spAtlas_dispose(atlas);
        return {"Failed to read Spine atlas: " + atlasPath, nullptr};
    }

    spAttachmentLoader* attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(atlas));
    spSkeletonData* skeletonData         = nullptr;
    const bool isJson                    = (firstPayloadByte(skelData) == static_cast<unsigned char>('{'));

    if (isJson)
    {
        std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                             static_cast<size_t>(skelData.getSize()));
        spSkeletonJson* reader = spSkeletonJson_createWithLoader(attachmentLoader);
        reader->scale          = scale;
        skeletonData           = spSkeletonJson_readSkeletonData(reader, jsonText.c_str());
        if (!skeletonData)
        {
            const std::string error = reader->error ? reader->error : ("Failed to read Spine JSON: " + jsonPath);
            spSkeletonJson_dispose(reader);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return {error, nullptr};
        }
        spSkeletonJson_dispose(reader);
    }
    else
    {
        spSkeletonBinary* reader = spSkeletonBinary_createWithLoader(attachmentLoader);
        reader->scale            = scale;
        skeletonData =
            spSkeletonBinary_readSkeletonData(reader, skelData.getBytes(), static_cast<int>(skelData.getSize()));
        if (!skeletonData)
        {
            const std::string error = reader->error ? reader->error : ("Failed to read Spine binary: " + jsonPath);
            spSkeletonBinary_dispose(reader);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return {error, nullptr};
        }
        spSkeletonBinary_dispose(reader);
    }

    auto* animation = new (std::nothrow) OwnedAtlasSpine34Animation();
    if (!animation)
    {
        spSkeletonData_dispose(skeletonData);
        spAttachmentLoader_dispose(attachmentLoader);
        spAtlas_dispose(atlas);
        return {"Failed to allocate Spine node", nullptr};
    }

    animation->takeAtlasOwnership(atlas, attachmentLoader);
    animation->initWithData(skeletonData, true);
    animation->autorelease();
    return {"", animation};
}

bool spine34IsNode(ax::Node* node)
{
    return asSpine34(node) != nullptr;
}

bool spine34IsValid(ax::Node* node)
{
    auto* animation = asSpine34(node);
    return animation && animation->getSkeleton() && animation->getSkeleton()->data;
}

void spine34CollectAnimations(ax::Node* node, std::vector<std::string>& out)
{
    auto* animation = asSpine34(node);
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->data)
        return;
    spSkeletonData* data = animation->getSkeleton()->data;
    for (int index = 0; index < data->animationsCount; ++index)
    {
        if (data->animations[index] && data->animations[index]->name)
            out.emplace_back(data->animations[index]->name);
    }
}

void spine34CollectSkins(ax::Node* node, std::vector<std::string>& out)
{
    auto* animation = asSpine34(node);
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->data)
        return;
    spSkeletonData* data = animation->getSkeleton()->data;
    for (int index = 0; index < data->skinsCount; ++index)
    {
        if (data->skins[index] && data->skins[index]->name)
            out.emplace_back(data->skins[index]->name);
    }
}

bool spine34HasAnimation(ax::Node* node, const std::string& name)
{
    auto* animation = asSpine34(node);
    if (!animation || name.empty())
        return false;
    return animation->findAnimation(name) != nullptr;
}

bool spine34HasSkin(ax::Node* node, const std::string& name)
{
    auto* animation = asSpine34(node);
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->data || name.empty())
        return false;
    return spSkeletonData_findSkin(animation->getSkeleton()->data, name.c_str()) != nullptr;
}

float spine34AnimationDuration(ax::Node* node, const std::string& name)
{
    auto* animation = asSpine34(node);
    if (!animation || name.empty())
        return 0.0f;
    spAnimation* found = animation->findAnimation(name);
    return found ? found->duration : 0.0f;
}

bool spine34SetAnimation(ax::Node* node, int trackIndex, const std::string& name, bool loop)
{
    auto* animation = asSpine34(node);
    if (!animation || name.empty())
        return false;
    return animation->setAnimation(trackIndex, name, loop) != nullptr;
}

void spine34KeepCurrentTrackAlive(ax::Node* node, int trackIndex)
{
    auto* animation = asSpine34(node);
    if (!animation)
        return;
    if (spTrackEntry* entry = animation->getCurrent(trackIndex))
        entry->endTime = std::numeric_limits<float>::max();
}

void spine34SeekCurrentTrack(ax::Node* node, int trackIndex, float timeSeconds)
{
    auto* animation = asSpine34(node);
    if (!animation)
        return;
    if (spTrackEntry* entry = animation->getCurrent(trackIndex))
    {
        entry->time     = timeSeconds;
        entry->lastTime = timeSeconds;
    }
}

void spine34SetSkin(ax::Node* node, const std::string& name)
{
    auto* animation = asSpine34(node);
    if (animation)
        animation->setSkin(name);
}

void spine34ClearTracks(ax::Node* node)
{
    auto* animation = asSpine34(node);
    if (animation)
        animation->clearTracks();
}

void spine34SetTimeScale(ax::Node* node, float scale)
{
    auto* animation = asSpine34(node);
    if (animation)
        animation->setTimeScale(scale);
}

void spine34SetDebugEnabled(ax::Node* node, bool bones, bool slots)
{
    auto* animation = asSpine34(node);
    if (!animation)
        return;
    animation->setDebugBonesEnabled(bones);
    animation->setDebugSlotsEnabled(slots);
}

void spine34Update(ax::Node* node, float dt)
{
    auto* animation = asSpine34(node);
    if (animation)
        animation->update(dt);
}
}  // namespace editor
