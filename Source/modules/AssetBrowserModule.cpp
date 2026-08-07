#include "modules/AssetBrowserModule.h"

#include "asset_browser/Asset.h"
#include "asset_browser/AssetBrowserService.h"
#include "asset_browser/AssetFileTypeRegistry.h"
#include "asset_browser/AssetSelectionService.h"
#include "core/EditorContext.h"
#include "core/EditorPreferencesService.h"
#include "core/FileRecycleBin.h"
#include "core/ModalDialogService.h"
#include "core/ProjectSettings.h"
#include "documents/IEditorDocument.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "platform/PlatformConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <system_error>
#include <utility>

namespace editor
{
namespace
{
std::string lowerExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

std::string layerFileNameWithExtension(std::string fileName)
{
    if (fileName.empty())
        fileName = "NewLayer.layer";

    std::filesystem::path path(fileName);
    if (lowerExtension(path) != ".layer")
        fileName += ".layer";
    return fileName;
}

bool containsPathSeparator(const std::string& value)
{
    return value.find('/') != std::string::npos || value.find('\\') != std::string::npos;
}

ImFont* iconFont()
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    if (!atlas || atlas->Fonts.Size == 0)
        return nullptr;

    for (ImFont* font : atlas->Fonts)
    {
        if (font && font->IsGlyphInFont(0xf07b) && font->IsGlyphInFont(0xf1c5))
            return font;
    }
    return nullptr;
}
}  // namespace

AssetBrowserModule::AssetBrowserModule() : PanelModule("Assets") {}

void AssetBrowserModule::drawContent(EditorContext& context)
{
    auto browser = context.services().get<AssetBrowserService>();
    if (!browser)
    {
        ImGui::TextDisabled("Asset browser service is not available.");
        return;
    }

    drawBrowser(context, *browser);
}

void AssetBrowserModule::drawBrowser(EditorContext& context, AssetBrowserService& browser)
{
    const std::string currentRoot =
        std::filesystem::path(context.settings().resourceRoot()).lexically_normal().generic_string();
    if (!m_stateLoaded || m_loadedRoot != currentRoot)
    {
        m_stateLoaded = true;
        m_loadedRoot  = currentRoot;
        m_rootOpen    = true;
        m_openAssets.clear();
        if (auto prefs = context.services().get<EditorPreferencesService>())
        {
            const AssetBrowserEntry entry = prefs->assetBrowserEntry(currentRoot);
            m_rootOpen                    = entry.rootOpen;
            m_openAssets                  = {entry.openPaths.begin(), entry.openPaths.end()};
        }
    }

    const std::filesystem::path root = assetRoot(context);
    if (!browser.rootExists(root))
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Content root does not exist.");
        ImGui::TextWrapped("%s", root.generic_string().c_str());
        return;
    }

    ImGui::TextDisabled("%s", root.generic_string().c_str());
    if (browser.isScanning())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Scanning...");
    }
    else if (!browser.statusMessage().empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", browser.statusMessage().c_str());
    }
    ImGui::Separator();

    if (ImGui::BeginChild("AssetTree", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (m_pendingRefresh)
        {
            std::string message;
            if (!browser.refresh(root, &message))
                m_statusMessage = message;
            else
                m_statusMessage = message;
            m_pendingRefresh = false;
        }

        const Asset& rootAsset = browser.rootAsset(root);
        m_visibleAssetPaths.clear();
        if (m_rootOpen)
        {
            for (const std::unique_ptr<Asset>& child : rootAsset.children)
            {
                if (child)
                    collectVisibleAssetPaths(*child);
            }
        }
        ImGui::PushID("ContentRoot");
        if (drawDisclosureTriangle(m_rootOpen))
        {
            m_rootOpen = !m_rootOpen;
            saveState(context);
        }
        ImGui::SameLine(0.0f, 2.0f);
        drawIcon(rootAsset.icon(m_rootOpen));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Selectable(rootAsset.displayName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            m_rootOpen = !m_rootOpen;
            saveState(context);
        }
        if (ImGui::IsItemHovered())
            rootAsset.drawTooltip();

        drawAssetContextMenu(context, browser, root, "ContentRootContextMenu", {}, true);
        ImGui::PopID();

        if (m_rootOpen)
        {
            ImGui::Indent(18.0f);
            for (const std::unique_ptr<Asset>& child : rootAsset.children)
            {
                if (child)
                    drawAssetNode(context, browser, root, *child);
            }
            ImGui::Unindent(18.0f);
        }
    }
    ImGui::EndChild();
}

void AssetBrowserModule::drawAssetNode(EditorContext& context,
                                       AssetBrowserService& browser,
                                       const std::filesystem::path& root,
                                       const Asset& asset)
{
    const std::string key = asset.relativePath.generic_string();
    ImGui::PushID(key.c_str());

    const bool expandable = asset.isDirectory() || !asset.children.empty();
    bool open             = m_openAssets.find(key) != m_openAssets.end();
    if (expandable)
    {
        if (drawDisclosureTriangle(open))
        {
            if (open)
                m_openAssets.erase(key);
            else
                m_openAssets.insert(key);
            open = !open;
            saveState(context);
        }
        ImGui::SameLine(0.0f, 2.0f);
    }
    else
    {
        ImGui::Indent(ImGui::GetFrameHeight() + 2.0f);
    }

    drawIcon(asset.icon(open));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    const auto selection = context.services().get<AssetSelectionService>();
    const bool selected  = selection && selection->isSelected(asset.relativePath);
    const bool activated =
        ImGui::Selectable(asset.displayName.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
    if (activated)
    {
        if (selection)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift)
                selection->selectRange(asset.relativePath, m_visibleAssetPaths);
            else if (io.KeyCtrl)
                selection->toggleAsset(asset.relativePath);
            else
                selection->selectAsset(asset.relativePath);
        }

        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (asset.canOpenDocument())
                openAsset(context, asset);
            else if (expandable)
            {
                if (open)
                    m_openAssets.erase(key);
                else
                    m_openAssets.insert(key);
                open = !open;
                saveState(context);
            }
        }
    }

    const bool realFile = asset.kind != AssetKind::SpriteFrame;
    const std::filesystem::path createTargetDirectory =
        asset.isDirectory() ? asset.relativePath : asset.relativePath.parent_path();
    drawAssetContextMenu(context, browser, root, "AssetContextMenu", createTargetDirectory, realFile, &asset);
    if (ImGui::BeginDragDropSource())
    {
        const bool dragSelection =
            selection && selection->isSelected(asset.relativePath) && selection->selectedAssetPaths().size() > 1;
        if (dragSelection)
        {
            AssetDragPayloadBatch batch;
            for (const std::filesystem::path& selectedPath : selection->selectedAssetPaths())
            {
                if (batch.count >= kMaxAssetDragBatchItems)
                    break;
                if (const Asset* selectedAsset = browser.findAsset(selectedPath))
                    selectedAsset->fillDragPayload(batch.items[batch.count++]);
            }
            ImGui::SetDragDropPayload(kAssetDragBatchPayloadType, &batch, sizeof(batch));
            ImGui::Text("%u assets", batch.count);
            if (selection->selectedAssetPaths().size() > kMaxAssetDragBatchItems)
                ImGui::TextDisabled("First %zu items", kMaxAssetDragBatchItems);
        }
        else
        {
            AssetDragPayload payload;
            asset.fillDragPayload(payload);
            ImGui::SetDragDropPayload(kAssetDragPayloadType, &payload, sizeof(payload));
            ImGui::TextUnformatted(asset.displayName.c_str());
            ImGui::TextDisabled("%s", asset.relativePath.generic_string().c_str());
        }
        ImGui::EndDragDropSource();
    }

    if (ImGui::IsItemHovered())
    {
        m_statusMessage = asset.relativePath.generic_string();
        asset.drawTooltip();
    }

    if (expandable && open)
    {
        ImGui::Indent(18.0f);
        for (const std::unique_ptr<Asset>& child : asset.children)
        {
            if (child)
                drawAssetNode(context, browser, root, *child);
        }
        ImGui::Unindent(18.0f);
    }

    if (!expandable)
        ImGui::Unindent(ImGui::GetFrameHeight() + 2.0f);
    ImGui::PopID();
}

void AssetBrowserModule::collectVisibleAssetPaths(const Asset& asset)
{
    m_visibleAssetPaths.push_back(asset.relativePath.lexically_normal());
    const std::string key = asset.relativePath.generic_string();
    if (m_openAssets.find(key) == m_openAssets.end())
        return;
    for (const std::unique_ptr<Asset>& child : asset.children)
    {
        if (child)
            collectVisibleAssetPaths(*child);
    }
}

void AssetBrowserModule::drawAssetContextMenu(EditorContext& context,
                                              AssetBrowserService& browser,
                                              const std::filesystem::path& root,
                                              const char* popupId,
                                              const std::filesystem::path& targetDirectory,
                                              bool allowCreate,
                                              const Asset* asset)
{
    if (!ImGui::BeginPopupContextItem(popupId))
        return;

    const bool isLayerFile = asset && asset->kind == AssetKind::Layer;
    if (isLayerFile)
    {
        if (ImGui::MenuItem("Duplicate..."))
            requestDuplicateLayer(context, *asset);
        if (ImGui::MenuItem("Rename..."))
            requestRenameLayer(context, *asset);
        if (ImGui::MenuItem("Delete..."))
            requestDeleteLayer(context, *asset);
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Refresh"))
    {
        std::string message;
        if (!browser.refresh(root, &message))
            m_statusMessage = message;
        else
            m_statusMessage = message;
    }

    if (allowCreate)
    {
        ImGui::Separator();
        if (ImGui::BeginMenu("Create"))
        {
            for (const AssetFileTypeDescriptor* type : AssetFileTypeRegistry::instance().creatableTypes())
            {
                if (type && type->creation && ImGui::MenuItem(type->creation->menuLabel.c_str()))
                    requestCreateAsset(context, *type, targetDirectory);
            }
            ImGui::EndMenu();
        }
    }
    ImGui::EndPopup();
}

void AssetBrowserModule::requestCreateAsset(EditorContext& context,
                                            const AssetFileTypeDescriptor& type,
                                            const std::filesystem::path& targetDirectory)
{
    auto modal = context.services().get<ModalDialogService>();
    if (!modal || !type.creation)
        return;

    m_createTargetDirectory = targetDirectory.lexically_normal();
    if (m_createTargetDirectory == ".")
        m_createTargetDirectory.clear();
    m_createAssetKind = type.kind;
    m_createAssetName = type.creation->defaultFileName;

    modal->open(type.creation->modalId, type.creation->modalTitle, [this](EditorContext& context) {
        ImGui::Text("Directory: %s",
                    m_createTargetDirectory.empty() ? "Content" : m_createTargetDirectory.generic_string().c_str());
        ImGui::InputText("File Name", &m_createAssetName);
        drawModalMessage(context);
    }, {{"Create", [this](EditorContext& context) { return createAssetFile(context); }}, {"Cancel", [](EditorContext&) {
        return true;
    }}});
}

void AssetBrowserModule::requestDuplicateLayer(EditorContext& context, const Asset& asset)
{
    auto modal = context.services().get<ModalDialogService>();
    if (!modal)
        return;

    m_duplicateSourceRelativePath = asset.relativePath.lexically_normal();
    m_duplicateLayerName          = nextDuplicateLayerFileName(asset.absolutePath);

    modal->open("asset.duplicate-layer", "Duplicate Layer File",
                [this](EditorContext& context) {
        ImGui::Text("Source: %s", m_duplicateSourceRelativePath.generic_string().c_str());
        ImGui::InputText("New File Name", &m_duplicateLayerName);
        drawModalMessage(context);
    },
                {{"Duplicate", [this](EditorContext& context) { return duplicateLayerFile(context); }},
                 {"Cancel", [](EditorContext&) { return true; }}});
}

void AssetBrowserModule::requestRenameLayer(EditorContext& context, const Asset& asset)
{
    auto modal = context.services().get<ModalDialogService>();
    if (!modal)
        return;

    m_renameSourceRelativePath = asset.relativePath.lexically_normal();
    m_renameLayerName          = asset.relativePath.filename().generic_string();

    modal->open("asset.rename-layer", "Rename Layer File", [this](EditorContext& context) {
        ImGui::Text("Source: %s", m_renameSourceRelativePath.generic_string().c_str());
        ImGui::InputText("New File Name", &m_renameLayerName);
        drawModalMessage(context);
    }, {{"Rename", [this](EditorContext& context) { return renameLayerFile(context); }}, {"Cancel", [](EditorContext&) {
        return true;
    }}});
}

void AssetBrowserModule::requestDeleteLayer(EditorContext& context, const Asset& asset)
{
    auto modal = context.services().get<ModalDialogService>();
    if (!modal)
        return;

    m_deleteSourceRelativePath = asset.relativePath.lexically_normal();

    modal->open("asset.delete-layer", "Delete Layer File", [this](EditorContext& context) {
        ImGui::Text("Delete layer file?");
        ImGui::TextWrapped("%s", m_deleteSourceRelativePath.generic_string().c_str());
#if AX_TARGET_PLATFORM == AX_PLATFORM_WIN32
        ImGui::TextDisabled("The file will be moved to the recycle bin.");
#else
        ImGui::TextDisabled("The file will be deleted permanently.");
#endif
        drawModalMessage(context);
    }, {{"Delete", [this](EditorContext& context) { return deleteLayerFile(context); }}, {"Cancel", [](EditorContext&) {
        return true;
    }}});
}

bool AssetBrowserModule::createAssetFile(EditorContext& context)
{
    const AssetFileTypeRegistry& registry = AssetFileTypeRegistry::instance();
    const AssetFileTypeDescriptor* type   = registry.findByKind(m_createAssetKind);
    if (!type || !type->creation || !type->creation->createDefaultFile)
    {
        setModalMessage(context, "Asset type cannot be created.");
        return false;
    }

    const std::filesystem::path root      = assetRoot(context);
    const std::filesystem::path directory = (root / m_createTargetDirectory).lexically_normal();
    std::filesystem::path fileName;
    std::string message;
    if (!registry.resolveCreateFileName(*type, m_createAssetName, fileName, message))
    {
        setModalMessage(context, message);
        return false;
    }

    const std::filesystem::path path = makeUniqueAssetPath(directory, fileName.generic_string());

    if (!registry.createDefaultFile(type->kind, path, message))
    {
        setModalMessage(context, message);
        return false;
    }

    if (type->creation->openAfterCreate)
    {
        std::unique_ptr<IEditorDocument> document = registry.createDocument(type->kind);
        if (!document || !document->open(path))
        {
            setModalMessage(context, document ? document->lastError() : "No document editor registered.");
            return false;
        }
        context.documents().openOrActivate(std::move(document));
    }

    if (!m_createTargetDirectory.empty())
        m_openAssets.insert(m_createTargetDirectory.generic_string());
    m_statusMessage  = "Created " + std::filesystem::relative(path, root).generic_string();
    m_pendingRefresh = true;
    return true;
}

bool AssetBrowserModule::duplicateLayerFile(EditorContext& context)
{
    const std::filesystem::path root            = assetRoot(context);
    const std::filesystem::path source          = (root / m_duplicateSourceRelativePath).lexically_normal();
    const std::filesystem::path targetDirectory = source.parent_path();
    std::filesystem::path fileName;
    std::string message;
    if (!resolveLayerFileName(m_duplicateLayerName, fileName, message))
    {
        setModalMessage(context, message);
        return false;
    }

    const std::filesystem::path target = (targetDirectory / fileName).lexically_normal();
    if (std::filesystem::exists(target))
    {
        setModalMessage(context, "Target file already exists.");
        return false;
    }

    std::error_code error;
    if (!std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, error) || error)
    {
        setModalMessage(context, "Failed to duplicate layer file: " + error.message());
        return false;
    }

    const std::filesystem::path relativeDirectory = m_duplicateSourceRelativePath.parent_path();
    if (!relativeDirectory.empty())
        m_openAssets.insert(relativeDirectory.generic_string());
    m_statusMessage  = "Duplicated " + std::filesystem::relative(target, root).generic_string();
    m_pendingRefresh = true;
    return true;
}

bool AssetBrowserModule::renameLayerFile(EditorContext& context)
{
    const std::filesystem::path root   = assetRoot(context);
    const std::filesystem::path source = (root / m_renameSourceRelativePath).lexically_normal();
    if (context.documents().findByPath(source))
    {
        setModalMessage(context, "Close the layer file before renaming it.");
        return false;
    }

    std::filesystem::path fileName;
    std::string message;
    if (!resolveLayerFileName(m_renameLayerName, fileName, message))
    {
        setModalMessage(context, message);
        return false;
    }

    const std::filesystem::path target = (source.parent_path() / fileName).lexically_normal();
    if (source == target)
    {
        setModalMessage(context, "New file name is the same as the current name.");
        return false;
    }
    if (std::filesystem::exists(target))
    {
        setModalMessage(context, "Target file already exists.");
        return false;
    }

    std::error_code error;
    std::filesystem::rename(source, target, error);
    if (error)
    {
        setModalMessage(context, "Failed to rename layer file: " + error.message());
        return false;
    }

    const std::filesystem::path relativeDirectory = m_renameSourceRelativePath.parent_path();
    if (!relativeDirectory.empty())
        m_openAssets.insert(relativeDirectory.generic_string());
    m_statusMessage  = "Renamed to " + std::filesystem::relative(target, root).generic_string();
    m_pendingRefresh = true;
    return true;
}

bool AssetBrowserModule::deleteLayerFile(EditorContext& context)
{
    const std::filesystem::path root   = assetRoot(context);
    const std::filesystem::path source = (root / m_deleteSourceRelativePath).lexically_normal();
    if (context.documents().findByPath(source))
    {
        setModalMessage(context, "Close the layer file before deleting it.");
        return false;
    }

    std::string message;
    if (!deletePathWithPlatformTrash(source, message))
    {
        setModalMessage(context, message);
        return false;
    }

    const std::filesystem::path relativeDirectory = m_deleteSourceRelativePath.parent_path();
    if (!relativeDirectory.empty())
        m_openAssets.insert(relativeDirectory.generic_string());
    m_statusMessage  = "Deleted " + m_deleteSourceRelativePath.generic_string();
    m_pendingRefresh = true;
    return true;
}

void AssetBrowserModule::openAsset(EditorContext& context, const Asset& asset)
{
    std::unique_ptr<IEditorDocument> document = AssetFileTypeRegistry::instance().createDocument(asset.kind);
    if (!document)
    {
        m_statusMessage = "No editor registered for " + asset.displayName;
        return;
    }

    if (!document->open(asset.absolutePath))
    {
        m_statusMessage = document->lastError();
        return;
    }

    bool activatedExisting = false;
    context.documents().openOrActivate(std::move(document), &activatedExisting);
    m_statusMessage = activatedExisting ? "Activated existing document." : "Opened " + asset.displayName;
}

void AssetBrowserModule::drawIcon(const char* icon) const
{
    if (ImFont* font = iconFont())
    {
        ImGui::PushFont(font, 0.0f);
        ImGui::TextUnformatted(icon);
        ImGui::PopFont();
        return;
    }

    ImGui::TextUnformatted(icon);
}

bool AssetBrowserModule::drawDisclosureTriangle(bool open) const
{
    const float frameHeight = ImGui::GetFrameHeight();
    const ImVec2 size(frameHeight, frameHeight);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size.x, min.y + size.y);

    const bool clicked   = ImGui::InvisibleButton("##toggle", size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (drawList)
    {
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const float radius = frameHeight * 0.22f;
        const ImU32 color  = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        if (open)
        {
            drawList->AddTriangleFilled(ImVec2(center.x - radius, center.y - radius * 0.45f),
                                        ImVec2(center.x + radius, center.y - radius * 0.45f),
                                        ImVec2(center.x, center.y + radius * 0.65f), color);
        }
        else
        {
            drawList->AddTriangleFilled(ImVec2(center.x - radius * 0.45f, center.y - radius),
                                        ImVec2(center.x - radius * 0.45f, center.y + radius),
                                        ImVec2(center.x + radius * 0.65f, center.y), color);
        }
    }
    return clicked;
}

void AssetBrowserModule::drawModalMessage(EditorContext& context) const
{
    auto modal = context.services().get<ModalDialogService>();
    if (modal && !modal->message().empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", modal->message().c_str());
}

void AssetBrowserModule::setModalMessage(EditorContext& context, std::string message) const
{
    auto modal = context.services().get<ModalDialogService>();
    if (modal)
        modal->setMessage(std::move(message));
}

std::filesystem::path AssetBrowserModule::makeUniqueAssetPath(const std::filesystem::path& directory,
                                                              const std::string& fileName) const
{
    const std::filesystem::path requested(fileName);
    const std::string stem          = requested.stem().generic_string();
    const std::string extension     = requested.extension().generic_string();
    std::filesystem::path candidate = directory / requested.filename();
    int suffix                      = 1;
    while (std::filesystem::exists(candidate))
    {
        candidate = directory / (stem + " " + std::to_string(suffix++) + extension);
    }
    return candidate.lexically_normal();
}

std::string AssetBrowserModule::nextDuplicateLayerFileName(const std::filesystem::path& sourcePath) const
{
    const std::filesystem::path directory = sourcePath.parent_path();
    const std::string stem                = sourcePath.stem().generic_string();
    const std::string extension = sourcePath.extension().empty() ? ".layer" : sourcePath.extension().generic_string();

    int suffix = 1;
    std::filesystem::path candidate;
    std::string candidateName;
    do
    {
        candidateName = stem + "_" + std::to_string(suffix++) + extension;
        candidate     = directory / candidateName;
    } while (std::filesystem::exists(candidate));
    return candidateName;
}

bool AssetBrowserModule::resolveLayerFileName(const std::string& requestedName,
                                              std::filesystem::path& fileName,
                                              std::string& message) const
{
    message.clear();
    if (requestedName.empty())
    {
        message = "File name is required.";
        return false;
    }
    if (containsPathSeparator(requestedName))
    {
        message = "File name cannot contain path separators.";
        return false;
    }

    fileName = std::filesystem::path(layerFileNameWithExtension(requestedName)).filename();
    if (fileName.empty() || fileName == "." || fileName == ".." || fileName.stem().empty())
    {
        message = "File name is invalid.";
        return false;
    }
    return true;
}

std::filesystem::path AssetBrowserModule::assetRoot(EditorContext& context) const
{
    return std::filesystem::path(context.settings().resourceRoot()).lexically_normal();
}

void AssetBrowserModule::saveState(EditorContext& context)
{
    auto prefs = context.services().get<EditorPreferencesService>();
    if (!prefs || m_loadedRoot.empty())
        return;
    AssetBrowserEntry entry;
    entry.rootOpen = m_rootOpen;
    entry.openPaths.assign(m_openAssets.begin(), m_openAssets.end());
    prefs->setAssetBrowserEntry(m_loadedRoot, entry);
}
}  // namespace editor
