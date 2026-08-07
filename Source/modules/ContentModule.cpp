#include "modules/ContentModule.h"

#include "core/EditorContext.h"
#include "document_editors/DocumentEditorRegistry.h"
#include "documents/DocumentCloseService.h"
#include "documents/LayerDocument.h"
#include "documents/TextDocument.h"
#include "layer/LayerSceneContentExtension.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

namespace editor
{
ContentModule::ContentModule() : PanelModule("Content") {}

ContentModule::~ContentModule() = default;

void ContentModule::onUpdate(EditorContext&, float deltaTime)
{
    m_sceneContentEditor.update(deltaTime);
}

void ContentModule::drawContent(EditorContext& context)
{
    drawDocumentTabs(context);
    if (!m_statusMessage.empty())
        ImGui::TextWrapped("%s", m_statusMessage.c_str());

    ImGui::Separator();
    drawActiveDocument(context);
}

void ContentModule::drawDocumentTabs(EditorContext& context)
{
    auto& documents = context.documents().documents();
    if (documents.empty())
    {
        m_lastTabActiveIndex = DocumentRegistry::npos;
        ImGui::TextDisabled("No document open.");
        return;
    }

    std::size_t pendingClose      = DocumentRegistry::npos;
    std::size_t selectedIndex     = context.documents().activeIndex();
    const bool externalActivation = context.documents().activeRevision() != m_seenDocumentActiveRevision;
    const bool forceSelectedTab   = externalActivation || selectedIndex != m_lastTabActiveIndex;
    if (ImGui::BeginTabBar("DocumentTabs", ImGuiTabBarFlags_Reorderable))
    {
        for (std::size_t index = 0; index < documents.size(); ++index)
        {
            auto& document    = documents[index];
            std::string label = document->getDisplayName();
            if (document->isDirty())
                label += "*";
            label += "###" + document->path().generic_string();

            bool open = true;
            const ImGuiTabItemFlags tabFlags =
                forceSelectedTab && selectedIndex == index ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(label.c_str(), &open, tabFlags))
            {
                if (!externalActivation)
                    selectedIndex = index;
                ImGui::EndTabItem();
            }

            if (!open)
                pendingClose = index;
        }
        ImGui::EndTabBar();
    }

    if (selectedIndex < documents.size() && context.documents().setActiveIndex(selectedIndex))
    {
        m_lastTabActiveIndex         = selectedIndex;
        m_seenDocumentActiveRevision = context.documents().activeRevision();
    }

    if (pendingClose != DocumentRegistry::npos)
    {
        if (auto closeService = context.services().get<DocumentCloseService>())
            closeService->requestClose(pendingClose);
        else
            m_statusMessage = "Document close service is not available.";
    }
}

void ContentModule::drawActiveDocument(EditorContext& context)
{
    IEditorDocument* activeDocument = context.documents().activeDocument();
    if (!activeDocument)
    {
        m_sceneContentEditor.reset();
        return;
    }

    if (auto registry = context.services().get<DocumentEditorRegistry>())
    {
        if (IDocumentEditor* editor = registry->find(*activeDocument))
        {
            m_sceneContentEditor.reset();
            editor->drawContent(context, *activeDocument);
            return;
        }
    }

    if (auto* layerDocument = dynamic_cast<LayerDocument*>(activeDocument))
    {
        drawLayerDocument(context, *layerDocument);
        return;
    }

    m_sceneContentEditor.reset();
    if (auto* textDocument = dynamic_cast<TextDocument*>(activeDocument))
    {
        drawTextDocument(*textDocument);
        return;
    }

    ImGui::TextDisabled("This document type has no editor yet.");
}

void ContentModule::drawLayerDocument(EditorContext& context, LayerDocument& document)
{
    SceneContentExtensionHooks hooks = makeLayerSceneContentExtensionHooks(document);
    m_sceneContentEditor.draw(context, document, document.getDisplayName(), document.path(), hooks);
}

void ContentModule::drawTextDocument(TextDocument& document)
{
    ImVec2 editorSize = ImGui::GetContentRegionAvail();
    if (editorSize.y < 120.0f)
        editorSize.y = 120.0f;

    if (ImGui::InputTextMultiline("##TextDocumentEditor", &document.content(), editorSize,
                                  ImGuiInputTextFlags_AllowTabInput))
    {
        document.markDirty();
    }
}
}  // namespace editor
