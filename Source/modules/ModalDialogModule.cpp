#include "modules/ModalDialogModule.h"

#include "core/EditorContext.h"
#include "core/ModalDialogService.h"
#include "imgui.h"

#include <cstddef>

namespace editor
{
void ModalDialogModule::onAttach(EditorContext&) {}

void ModalDialogModule::onDetach(EditorContext&) {}

void ModalDialogModule::onUpdate(EditorContext&, float) {}

void ModalDialogModule::onImGuiRender(EditorContext& context)
{
    auto modal = context.services().get<ModalDialogService>();
    if (!modal || !modal->hasDialog())
        return;

    const std::string title = modal->title();
    if (modal->consumeOpenRequest())
        ImGui::OpenPopup(title.c_str());

    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    modal->renderBody(context);

    const auto buttons = modal->buttons();
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        if (i > 0)
            ImGui::SameLine();

        if (ImGui::Button(buttons[i].label.c_str()))
        {
            const bool closeDialog = !buttons[i].callback || buttons[i].callback(context);
            if (closeDialog)
            {
                ImGui::CloseCurrentPopup();
                modal->close();
                break;
            }
        }
    }

    ImGui::EndPopup();
}
}  // namespace editor
