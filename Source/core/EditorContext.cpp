#include "core/EditorContext.h"

namespace editor
{
ServiceRegistry& EditorContext::services()
{
    return m_services;
}

ProjectSettings& EditorContext::settings()
{
    return m_settings;
}

ModuleHost& EditorContext::modules()
{
    return m_modules;
}

DocumentRegistry& EditorContext::documents()
{
    return m_documents;
}

ToolRegistry& EditorContext::tools()
{
    return m_tools;
}
}  // namespace editor
