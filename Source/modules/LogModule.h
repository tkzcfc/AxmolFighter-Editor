#pragma once

#include "modules/IEditorModule.h"

#include <cstdint>

namespace editor
{
class LogModule final : public IEditorModule
{
public:
    static constexpr const char* PanelId = "Log";

    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

private:
    std::uint64_t m_lastRevision = 0;
};
}  // namespace editor
