#include "spine/SafeSkeletonAnimation.h"

#include "spine/SkeletonAnimation.h"
#include "spine/SkeletonBinary.h"
#include "spine/SkeletonData.h"
#include "spine/SkeletonJson.h"
#include "spine/spine-axmol.h"

#include "platform/FileUtils.h"

#include <new>
#include <string>

namespace editor
{
namespace
{
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
}  // namespace

std::tuple<std::string, spine::SkeletonAnimation*> createSafeSkeletonAnimation(
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
}  // namespace editor
