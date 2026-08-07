#pragma once

#include "core/IService.h"

#include <functional>
#include <string>
#include <vector>

namespace editor
{
class EditorContext;

class ModalDialogService final : public IService
{
public:
    using BodyRenderer = std::function<void(EditorContext&)>;
    using ButtonCallback = std::function<bool(EditorContext&)>;

    struct Button
    {
        std::string label;
        ButtonCallback callback;
    };

    void open(std::string id,
              std::string title,
              BodyRenderer body,
              std::vector<Button> buttons);
    void close();

    bool hasDialog() const;
    bool isOpen(const std::string& id) const;
    bool consumeOpenRequest();

    const std::string& id() const;
    const std::string& title() const;
    const std::string& message() const;
    const std::vector<Button>& buttons() const;

    void setMessage(std::string message);
    void clearMessage();
    void renderBody(EditorContext& context);

private:
    std::string m_id;
    std::string m_title;
    std::string m_message;
    BodyRenderer m_body;
    std::vector<Button> m_buttons;
    bool m_hasDialog = false;
    bool m_openRequested = false;
};
}  // namespace editor
