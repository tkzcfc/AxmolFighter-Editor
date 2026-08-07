#include "modules/DocumentCloseDialogModule.h"

#include "core/ModalDialogService.h"
#include "core/EditorContext.h"
#include "documents/DocumentCloseService.h"
#include "documents/IEditorDocument.h"
#include "imgui.h"

namespace editor
{
namespace
{
constexpr const char* kDialogId = "document.close-unsaved";
constexpr const char* kDialogTitle = "Unsaved Document";
}

void DocumentCloseDialogModule::onAttach(EditorContext&) {}

void DocumentCloseDialogModule::onDetach(EditorContext&) {}

void DocumentCloseDialogModule::onUpdate(EditorContext&, float) {}

void DocumentCloseDialogModule::onImGuiRender(EditorContext& context)
{
    auto closeService = context.services().get<DocumentCloseService>();
    if (!closeService || !closeService->hasRequest())
        return;

    const std::size_t index = closeService->requestedIndex();
    auto& documents = context.documents().documents();
    if (index >= documents.size())
    {
        closeService->clearRequest();
        return;
    }

    IEditorDocument* document = documents[index].get();
    if (!document->isDirty())
    {
        closeRequestedDocument(context, false);
        return;
    }

    auto modal = context.services().get<ModalDialogService>();
    if (!modal || modal->hasDialog())
        return;

    modal->open(kDialogId,
                kDialogTitle,
                [](EditorContext& context) {
                    auto closeService = context.services().get<DocumentCloseService>();
                    if (!closeService || !closeService->hasRequest())
                        return;

                    const std::size_t index = closeService->requestedIndex();
                    auto& documents = context.documents().documents();
                    if (index >= documents.size())
                        return;

                    IEditorDocument* document = documents[index].get();
                    ImGui::Text("Save changes to %s before closing?", document->getDisplayName().c_str());
                    if (!closeService->message().empty())
                    {
                        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f),
                                           "%s",
                                           closeService->message().c_str());
                    }
                },
                {{"Save",
                  [this](EditorContext& context) {
                      auto closeService = context.services().get<DocumentCloseService>();
                      if (!closeService || !closeService->hasRequest())
                          return true;

                      const std::size_t index = closeService->requestedIndex();
                      auto& documents = context.documents().documents();
                      if (index >= documents.size())
                      {
                          closeService->clearRequest();
                          return true;
                      }

                      IEditorDocument* document = documents[index].get();
                      if (document->save())
                      {
                          closeRequestedDocument(context, true);
                          return true;
                      }

                      closeService->setMessage("Save failed: " + document->lastError());
                      return false;
                  }},
                 {"Discard",
                  [this](EditorContext& context) {
                      closeRequestedDocument(context, true);
                      return true;
                  }},
                 {"Cancel",
                  [](EditorContext& context) {
                      auto closeService = context.services().get<DocumentCloseService>();
                      if (closeService)
                          closeService->clearRequest();
                      return true;
                  }}});
}

void DocumentCloseDialogModule::closeRequestedDocument(EditorContext& context, bool discardDirty)
{
    auto closeService = context.services().get<DocumentCloseService>();
    if (!closeService)
        return;

    std::string message;
    context.documents().closeDocument(closeService->requestedIndex(), message, discardDirty);
    closeService->clearRequest();
}
}  // namespace editor
