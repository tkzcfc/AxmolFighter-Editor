#pragma once

#include "core/ProjectSettings.h"
#include "core/ServiceRegistry.h"
#include "documents/DocumentRegistry.h"
#include "modules/ModuleHost.h"
#include "tools/ToolRegistry.h"

namespace editor
{
class EditorContext
{
public:
    ServiceRegistry& services();
    ProjectSettings& settings();
    ModuleHost& modules();
    DocumentRegistry& documents();
    ToolRegistry& tools();

private:
    ServiceRegistry m_services;
    ProjectSettings m_settings;
    ModuleHost m_modules;
    DocumentRegistry m_documents;
    ToolRegistry m_tools;
};
}  // namespace editor
