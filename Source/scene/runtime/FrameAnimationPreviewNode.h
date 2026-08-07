#pragma once

#include "animation/AnimationTypes.h"
#include "2d/Node.h"

#include <map>
#include <string>
#include <vector>

namespace ax
{
class Node;
class Sprite;
class SpriteFrame;
}  // namespace ax

namespace editor
{
class FrameAnimationPreviewNode final : public ax::Node
{
public:
    ~FrameAnimationPreviewNode() override;

    static FrameAnimationPreviewNode* create(const std::string& aniPath,
                                             const AnimationPreviewVariables& variables,
                                             bool loop,
                                             bool playing,
                                             const std::string& blendSrc,
                                             const std::string& blendDst,
                                             std::string& error);

    bool configure(const std::string& aniPath,
                   const AnimationPreviewVariables& variables,
                   bool loop,
                   bool playing,
                   const std::string& blendSrc,
                   const std::string& blendDst,
                   std::string& error);

    void update(float delta) override;

private:
    struct PreviewFrame
    {
        ax::SpriteFrame* spriteFrame = nullptr;
        AnimationFrameData data;
    };

    bool init() override;
    void clearFrames();
    void applyCurrentFrame();

    ax::Sprite* m_sprite = nullptr;
    std::vector<PreviewFrame> m_frames;
    std::string m_aniPath;
    AnimationPreviewVariables m_variables;
    std::string m_blendSrc;
    std::string m_blendDst;
    std::size_t m_currentFrame = 0;
    double m_elapsedMs         = 0.0;
    bool m_loop                = true;
    bool m_playing             = true;
};
}  // namespace editor
