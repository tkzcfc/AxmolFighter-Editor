#include "core/ModalDialogService.h"

#include "core/EditorContext.h"

#include <utility>

namespace editor
{
void ModalDialogService::open(std::string id,
                              std::string title,
                              BodyRenderer body,
                              std::vector<Button> buttons)
{
    m_id = std::move(id);
    m_title = std::move(title);
    m_body = std::move(body);
    m_buttons = std::move(buttons);
    m_message.clear();
    m_hasDialog = true;
    m_openRequested = true;
}

void ModalDialogService::close()
{
    m_id.clear();
    m_title.clear();
    m_message.clear();
    m_body = nullptr;
    m_buttons.clear();
    m_hasDialog = false;
    m_openRequested = false;
}

bool ModalDialogService::hasDialog() const
{
    return m_hasDialog;
}

bool ModalDialogService::isOpen(const std::string& id) const
{
    return m_hasDialog && m_id == id;
}

bool ModalDialogService::consumeOpenRequest()
{
    const bool requested = m_openRequested;
    m_openRequested = false;
    return requested;
}

const std::string& ModalDialogService::id() const
{
    return m_id;
}

const std::string& ModalDialogService::title() const
{
    return m_title;
}

const std::string& ModalDialogService::message() const
{
    return m_message;
}

const std::vector<ModalDialogService::Button>& ModalDialogService::buttons() const
{
    return m_buttons;
}

void ModalDialogService::setMessage(std::string message)
{
    m_message = std::move(message);
}

void ModalDialogService::clearMessage()
{
    m_message.clear();
}

void ModalDialogService::renderBody(EditorContext& context)
{
    if (m_body)
        m_body(context);
}
}  // namespace editor
