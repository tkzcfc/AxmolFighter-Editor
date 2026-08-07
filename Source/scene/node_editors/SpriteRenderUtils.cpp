#include "scene/node_editors/SpriteRenderUtils.h"

#include "2d/Sprite.h"
#include "2d/SpriteFrame.h"
#include "renderer/backend/Enums.h"

#include <algorithm>
#include <cmath>
#include <new>

namespace editor
{
namespace
{
void setSpriteQuad(ax::V3F_C4B_T2F_Quad* quad,
                   const ax::Size& originalSize,
                   int x,
                   int y,
                   float xFactor,
                   float yFactor)
{
    const float offsetX = originalSize.width * x;
    const float offsetY = originalSize.height * y;

    quad->bl.vertices.set(ax::Vec3(offsetX, offsetY, 0.0f));
    quad->br.vertices.set(ax::Vec3(offsetX + originalSize.width * xFactor, offsetY, 0.0f));
    quad->tl.vertices.set(ax::Vec3(offsetX, offsetY + originalSize.height * yFactor, 0.0f));
    quad->tr.vertices.set(ax::Vec3(offsetX + originalSize.width * xFactor, offsetY + originalSize.height * yFactor, 0.0f));

    if (xFactor != 1.0f || yFactor != 1.0f)
    {
        const float xSize = (quad->br.texCoords.u - quad->bl.texCoords.u) * xFactor;
        const float ySize = (quad->tl.texCoords.v - quad->bl.texCoords.v) * yFactor;

        quad->br.texCoords = ax::Tex2F(quad->bl.texCoords.u + xSize, quad->bl.texCoords.v);
        quad->tl.texCoords = ax::Tex2F(quad->tl.texCoords.u, quad->bl.texCoords.v + ySize);
        quad->tr.texCoords = ax::Tex2F(quad->bl.texCoords.u + xSize, quad->bl.texCoords.v + ySize);
    }
}

void tileSprite(ax::Sprite& sprite)
{
    ax::SpriteFrame* frame = sprite.getSpriteFrame();
    if (!frame)
        return;

    const ax::Size newSize = sprite.getContentSize();
    const ax::Size originalSizePixels = frame->getOriginalSizeInPixels();
    const ax::Rect originalRect = frame->getRectInPixels();
    if (originalSizePixels.fuzzyEquals(ax::Vec2::ZERO, 0.0001f) || originalRect.size.width <= 0.0f ||
        originalRect.size.height <= 0.0f)
    {
        return;
    }

    sprite.setContentSize(originalSizePixels);
    ax::V3F_C4B_T2F_Quad originalQuad = sprite.getQuad();
    sprite.setContentSize(newSize);

    const float xRatio = newSize.width / originalRect.size.width;
    const float yRatio = newSize.height / originalRect.size.height;
    const int xCount = std::max(1, static_cast<int>(std::ceil(xRatio)));
    const int yCount = std::max(1, static_cast<int>(std::ceil(yRatio)));
    const int totalQuads = xCount * yCount;

    auto* quads = new (std::nothrow) ax::V3F_C4B_T2F_Quad[totalQuads];
    auto* indices = new (std::nothrow) unsigned short[totalQuads * 6];
    if (!quads || !indices)
    {
        delete[] quads;
        delete[] indices;
        return;
    }

    for (int y = 0; y < yCount; ++y)
    {
        for (int x = 0; x < xCount; ++x)
        {
            const int index = y * xCount + x;
            quads[index] = originalQuad;
            const float xFactor = originalRect.size.width * (x + 1) <= newSize.width ? 1.0f : xRatio - std::floor(xRatio);
            const float yFactor = originalRect.size.height * (y + 1) <= newSize.height ? 1.0f : yRatio - std::floor(yRatio);
            setSpriteQuad(&quads[index], originalRect.size, x, y, xFactor, yFactor);
        }
    }

    for (int index = 0; index < totalQuads; ++index)
    {
        indices[index * 6 + 0] = static_cast<unsigned short>(index * 4 + 0);
        indices[index * 6 + 1] = static_cast<unsigned short>(index * 4 + 1);
        indices[index * 6 + 2] = static_cast<unsigned short>(index * 4 + 2);
        indices[index * 6 + 3] = static_cast<unsigned short>(index * 4 + 3);
        indices[index * 6 + 4] = static_cast<unsigned short>(index * 4 + 2);
        indices[index * 6 + 5] = static_cast<unsigned short>(index * 4 + 1);
    }

    ax::PolygonInfo polygonInfo;
    polygonInfo.triangles.vertCount = 4 * totalQuads;
    polygonInfo.triangles.indexCount = 6 * totalQuads;
    polygonInfo.triangles.verts = reinterpret_cast<ax::V3F_C4B_T2F*>(quads);
    polygonInfo.triangles.indices = indices;
    sprite.setPolygonInfo(polygonInfo);
}

ax::backend::BlendFactor blendFactorFromName(const std::string& value, ax::backend::BlendFactor fallback)
{
    using ax::backend::BlendFactor;
    if (value == "ZERO")
        return BlendFactor::ZERO;
    if (value == "ONE")
        return BlendFactor::ONE;
    if (value == "SRC_COLOR")
        return BlendFactor::SRC_COLOR;
    if (value == "ONE_MINUS_SRC_COLOR")
        return BlendFactor::ONE_MINUS_SRC_COLOR;
    if (value == "SRC_ALPHA")
        return BlendFactor::SRC_ALPHA;
    if (value == "ONE_MINUS_SRC_ALPHA")
        return BlendFactor::ONE_MINUS_SRC_ALPHA;
    if (value == "DST_COLOR")
        return BlendFactor::DST_COLOR;
    if (value == "ONE_MINUS_DST_COLOR")
        return BlendFactor::ONE_MINUS_DST_COLOR;
    if (value == "DST_ALPHA")
        return BlendFactor::DST_ALPHA;
    if (value == "ONE_MINUS_DST_ALPHA")
        return BlendFactor::ONE_MINUS_DST_ALPHA;
    if (value == "SRC_ALPHA_SATURATE")
        return BlendFactor::SRC_ALPHA_SATURATE;
    if (value == "BLEND_COLOR")
        return BlendFactor::BLEND_COLOR;
    if (value == "CONSTANT_ALPHA")
        return BlendFactor::CONSTANT_ALPHA;
    if (value == "ONE_MINUS_CONSTANT_ALPHA")
        return BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
    return fallback;
}

ax::Rect normalizedSliceCenterRect(const SpriteNodeData& data)
{
    const float x = std::clamp(data.sliceCenterOrigin.x, 0.0f, 1.0f);
    const float y = std::clamp(data.sliceCenterOrigin.y, 0.0f, 1.0f);
    const float width = std::clamp(data.sliceCenterSize.x, 0.0f, 1.0f - x);
    const float height = std::clamp(data.sliceCenterSize.y, 0.0f, 1.0f - y);
    return ax::Rect(x, y, width, height);
}
}  // namespace

const std::vector<std::string>& spriteRenderTypeNames()
{
    static const std::vector<std::string> names = {"Simple", "Tiled", "Sliced"};
    return names;
}

const std::vector<std::string>& spriteBlendFactorNames()
{
    static const std::vector<std::string> names = {"ZERO",
                                                   "ONE",
                                                   "SRC_COLOR",
                                                   "ONE_MINUS_SRC_COLOR",
                                                   "SRC_ALPHA",
                                                   "ONE_MINUS_SRC_ALPHA",
                                                   "DST_COLOR",
                                                   "ONE_MINUS_DST_COLOR",
                                                   "DST_ALPHA",
                                                   "ONE_MINUS_DST_ALPHA",
                                                   "SRC_ALPHA_SATURATE",
                                                   "BLEND_COLOR",
                                                   "CONSTANT_ALPHA",
                                                   "ONE_MINUS_CONSTANT_ALPHA"};
    return names;
}

std::string normalizeSpriteRenderType(const std::string& value)
{
    if (value == "Tiled" || value == "Sliced")
        return value;
    return "Simple";
}

std::string normalizeSpriteBlendFactor(const std::string& value, const std::string& fallback)
{
    if (value.empty())
        return fallback;

    for (const std::string& name : spriteBlendFactorNames())
    {
        if (value == name)
            return value;
    }
    return fallback;
}

void applySpriteBlendFunc(ax::Sprite& sprite, const std::string& blendSrc, const std::string& blendDst)
{
    const ax::BlendFunc defaultBlendFunc = sprite.getBlendFunc();
    sprite.setBlendFunc(
        {blendFactorFromName(blendSrc, defaultBlendFunc.src), blendFactorFromName(blendDst, defaultBlendFunc.dst)});
}

void applySpriteRenderSettings(ax::Sprite& sprite, const SpriteNodeData& data)
{
    applySpriteBlendFunc(sprite, data.blendSrc, data.blendDst);

    const std::string renderType = normalizeSpriteRenderType(data.renderType);
    if (renderType == "Tiled")
        tileSprite(sprite);
    else if (renderType == "Sliced")
        sprite.setCenterRectNormalized(normalizedSliceCenterRect(data));
    else
        sprite.setCenterRectNormalized(ax::Rect(0.0f, 0.0f, 1.0f, 1.0f));
}
}  // namespace editor
