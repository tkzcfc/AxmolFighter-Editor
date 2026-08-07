#pragma once

#include "scene/SceneContentEditor.h"

namespace editor
{
class LayerDocument;

SceneContentExtensionHooks makeLayerSceneContentExtensionHooks(LayerDocument& document);
}  // namespace editor
