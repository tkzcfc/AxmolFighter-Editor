#include "modules/InspectorModule.h"

#include "core/EditorContext.h"
#include "document_editors/DocumentEditorRegistry.h"
#include "documents/IEditorDocument.h"
#include "documents/LayerDocument.h"
#include "scene/node_editors/SceneNodeEditor.h"
#include "layer/LayerPluginEditor.h"
#include "scene/SceneDocument.h"
#include "imgui.h"

namespace editor
{
InspectorModule::InspectorModule() : PanelModule("Inspector") {}

void InspectorModule::drawContent(EditorContext& context)
{
    IEditorDocument* document = context.documents().activeDocument();
    if (!document)
    {
        ImGui::TextDisabled("No selection.");
        return;
    }

    if (auto registry = context.services().get<DocumentEditorRegistry>())
    {
        if (IDocumentEditor* editor = registry->find(*document))
        {
            editor->drawInspector(context, *document);
            return;
        }
    }

    if (auto* layerDocument = dynamic_cast<LayerDocument*>(document))
    {
        if (layerDocument->hasShapeSelection())
        {
            if (const LayerPluginEditor* shapeEditor = LayerPluginEditorRegistry::instance().find("shape"))
                shapeEditor->drawInspector(*layerDocument, m_activeSceneEditKey);
            else
                ImGui::TextDisabled("Shape plugin is not registered.");
            return;
        }
    }

    if (auto* sceneDocument = dynamic_cast<SceneDocument*>(document))
    {
        SceneNode* node = sceneDocument->selectedNode();
        if (!node)
        {
            ImGui::TextDisabled("No node selected.");
            return;
        }

        const SceneNodeEditorRegistry& registry = SceneNodeEditorRegistry::instance();
        const SceneNodeEditor* editor = registry.find(node->type);
        if (editor)
        {
            editor->drawInspector(*sceneDocument, *node, m_activeSceneEditKey);
        }
        else
        {
            registry.fallbackEditor().drawInspector(*sceneDocument, *node, m_activeSceneEditKey);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.36f, 0.28f, 1.0f), "Unknown node type: %s", node->type.c_str());
        }
        return;
    }

    ImGui::Text("Document");
    ImGui::Separator();
    ImGui::Text("Name: %s", document->getDisplayName().c_str());
    ImGui::TextWrapped("Path: %s", document->path().generic_string().c_str());
    ImGui::Text("Dirty: %s", document->isDirty() ? "Yes" : "No");
}
}  // namespace editor
