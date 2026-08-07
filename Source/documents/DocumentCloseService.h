#pragma once

#include "core/IService.h"
#include "documents/DocumentRegistry.h"

#include <cstddef>
#include <string>

namespace editor
{
class DocumentCloseService final : public IService
{
public:
    void requestClose(std::size_t documentIndex);
    void clearRequest();
    bool hasRequest() const;
    std::size_t requestedIndex() const;
    void setMessage(std::string message);
    const std::string& message() const;

private:
    std::size_t m_requestedIndex = DocumentRegistry::npos;
    std::string m_message;
};
}  // namespace editor
