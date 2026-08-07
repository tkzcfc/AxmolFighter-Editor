#include "scene/EditorRootScene.h"

#include "core/EditorPreferencesService.h"
#include "ImGui/ImGuiPresenter.h"
#include "imgui.h"

#include <cassert>

namespace editor
{
namespace
{
constexpr std::string_view kImGuiLoopId = "#axed";

void maximizeEditorWindow()
{
#ifdef AX_PLATFORM_PC
    auto* renderView = dynamic_cast<ax::RenderViewImpl*>(ax::Director::getInstance()->getRenderView());
    if (renderView && renderView->getWindow())
        glfwMaximizeWindow(renderView->getWindow());
#endif
}

void useEditorTextFontByDefault()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontAtlas* atlas = io.Fonts;
    if (!atlas)
        return;

    for (ImFont* font : atlas->Fonts)
    {
        if (!font)
            continue;

        const bool hasTextGlyphs = font->IsGlyphInFont('A') && font->IsGlyphInFont('a');
        const bool hasFontAwesomeGlyphs = font->IsGlyphInFont(0xf07b) || font->IsGlyphInFont(0xf06e);
        if (hasTextGlyphs && !hasFontAwesomeGlyphs)
        {
            io.FontDefault = font;
            return;
        }
    }
}
}  // namespace

bool EditorRootScene::init()
{
    if (!Scene::init())
        return false;

    scheduleUpdate();
    return true;
}

void EditorRootScene::onEnter()
{
    Scene::onEnter();
    m_runtime.initialize();
    configureImGui();
    ax::extension::ImGuiPresenter::getInstance()->addRenderLoop(
        kImGuiLoopId, AX_CALLBACK_0(EditorRootScene::drawImGui, this), this);
    scheduleOnce([](float) {
        maximizeEditorWindow();
    }, 0.0f, "editor.maximizeWindow");
}

void EditorRootScene::onExit()
{
    ax::extension::ImGuiPresenter::getInstance()->removeRenderLoop(kImGuiLoopId);
    m_runtime.shutdown();
    ax::extension::ImGuiPresenter::getInstance()->clearFonts();
    Scene::onExit();
}

void EditorRootScene::update(float deltaTime)
{
    m_runtime.update(deltaTime);
}

void EditorRootScene::drawImGui()
{
    useEditorTextFontByDefault();
    m_runtime.renderImGui();
}

void EditorRootScene::configureImGui()
{
    if (m_imguiConfigured)
        return;

    auto* presenter = ax::extension::ImGuiPresenter::getInstance();
    presenter->setViewResolution(1600.0f, 900.0f);
    presenter->clearFonts();

    auto preferences = m_runtime.context().services().get<EditorPreferencesService>();
    assert(preferences);
    const std::string editorFontPath = preferences->editorFontPath();
    const float editorFontSize = preferences->editorFontSize();

    std::string resolvedFontPath;
    if (ax::FileUtils* fileUtils = ax::FileUtils::getInstance())
        resolvedFontPath = fileUtils->fullPathForFilename(editorFontPath);
    if (!resolvedFontPath.empty())
        presenter->addFont(resolvedFontPath, editorFontSize);
    presenter->addFont(ax::FileUtils::getInstance()->fullPathForFilename("fonts/fa-solid-900.ttf"));
    presenter->enableDPIScale();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    m_imguiConfigured = true;
}
}  // namespace editor
