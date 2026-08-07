#include "modules/ShapeListModule.h"

#include "core/EditorContext.h"
#include "documents/IEditorDocument.h"
#include "documents/LayerDocument.h"
#include "imgui.h"
#include "layer/LayerPluginEditor.h"

namespace editor
{
ShapeListModule::ShapeListModule() : PanelModule("Shape List") {}

void ShapeListModule::drawContent(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    auto* layerDocument = dynamic_cast<LayerDocument*>(document);
    if (!layerDocument)
    {
        ImGui::TextDisabled("No layer document.");
        return;
    }

    const LayerPluginEditor* shapeEditor = LayerPluginEditorRegistry::instance().find("shape");
    if (!shapeEditor)
    {
        ImGui::TextDisabled("Shape plugin is not registered.");
        return;
    }

    shapeEditor->drawList(context, *layerDocument);
}
}  // namespace editor
