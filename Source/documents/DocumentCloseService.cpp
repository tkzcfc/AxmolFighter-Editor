#include "documents/DocumentCloseService.h"

#include <utility>

namespace editor
{
void DocumentCloseService::requestClose(std::size_t documentIndex)
{
    m_requestedIndex = documentIndex;
    m_message.clear();
}

void DocumentCloseService::clearRequest()
{
    m_requestedIndex = DocumentRegistry::npos;
    m_message.clear();
}

bool DocumentCloseService::hasRequest() const
{
    return m_requestedIndex != DocumentRegistry::npos;
}

std::size_t DocumentCloseService::requestedIndex() const
{
    return m_requestedIndex;
}

void DocumentCloseService::setMessage(std::string message)
{
    m_message = std::move(message);
}

const std::string& DocumentCloseService::message() const
{
    return m_message;
}
}  // namespace editor
