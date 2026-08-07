#include "tools/NpkExporterService.h"

#include "platform/Image.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "yasio/obstream.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace editor
{
namespace
{

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string normalizeSlashes(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

bool loadWhitelistFile(const std::filesystem::path& path,
                       std::unordered_set<std::string>& whitelist,
                       std::string& error)
{
    whitelist.clear();
    if (path.empty())
        return true;

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "Failed to open whitelist: " + path.generic_string();
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    rapidjson::Document root;
    root.Parse(json.c_str());
    if (root.HasParseError())
    {
        error = std::string("Invalid whitelist JSON: ") + rapidjson::GetParseError_En(root.GetParseError());
        return false;
    }
    if (!root.IsObject() || !root.HasMember("imgs") || !root["imgs"].IsArray())
    {
        error = "Whitelist JSON must contain an imgs array.";
        return false;
    }
    for (const auto& value : root["imgs"].GetArray())
    {
        if (!value.IsString())
        {
            error = "Whitelist imgs entries must be strings.";
            return false;
        }
        whitelist.insert(normalizeSlashes(value.GetString()));
    }
    return true;
}

bool passesImgSelection(const std::string& imgName,
                        const std::string& imgFilter,
                        const std::unordered_set<std::string>& whitelist)
{
    if (!imgFilter.empty() && imgName.find(imgFilter) == std::string::npos)
        return false;
    if (!whitelist.empty() && whitelist.find(imgName) == whitelist.end())
        return false;
    return true;
}

struct FailedImgEntry
{
    std::string img;
    std::string error;
};

void loadExistingFailedEntries(const std::filesystem::path& path, std::unordered_map<std::string, std::string>& byImg)
{
    byImg.clear();
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
        return;

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    rapidjson::Document root;
    root.Parse(buffer.str().c_str());
    if (root.HasParseError() || !root.IsObject())
        return;

    if (root.HasMember("failures") && root["failures"].IsArray())
    {
        for (const auto& value : root["failures"].GetArray())
        {
            if (!value.IsObject() || !value.HasMember("img") || !value["img"].IsString())
                continue;
            const std::string img = normalizeSlashes(value["img"].GetString());
            std::string message;
            if (value.HasMember("error") && value["error"].IsString())
                message = value["error"].GetString();
            byImg[img] = std::move(message);
        }
        return;
    }

    if (root.HasMember("imgs") && root["imgs"].IsArray())
    {
        for (const auto& value : root["imgs"].GetArray())
        {
            if (!value.IsString())
                continue;
            byImg.emplace(normalizeSlashes(value.GetString()), "");
        }
    }
}

bool writeFailedListFile(const std::filesystem::path& path,
                         const std::vector<FailedImgEntry>& failures,
                         bool merge,
                         std::size_t& mergedCount,
                         std::size_t& addedCount,
                         std::string& error)
{
    mergedCount = 0;
    addedCount = 0;
    if (path.empty())
    {
        error = "Failed-list path is empty.";
        return false;
    }

    std::error_code directoryError;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), directoryError);
        if (directoryError)
        {
            error = "Failed to create directory for failed list: " + path.parent_path().generic_string();
            return false;
        }
    }

    std::unordered_map<std::string, std::string> byImg;
    if (merge)
        loadExistingFailedEntries(path, byImg);

    for (const FailedImgEntry& entry : failures)
    {
        const std::string key = normalizeSlashes(entry.img);
        const auto inserted = byImg.emplace(key, entry.error);
        if (inserted.second)
            ++addedCount;
        else
            inserted.first->second = entry.error; // refresh error text for known IMG
    }

    std::vector<FailedImgEntry> unique;
    unique.reserve(byImg.size());
    for (auto& [img, message] : byImg)
        unique.push_back({img, std::move(message)});
    std::sort(unique.begin(), unique.end(),
              [](const FailedImgEntry& left, const FailedImgEntry& right) { return left.img < right.img; });
    mergedCount = unique.size();

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("version");
    writer.Int(1);
    writer.Key("imgs");
    writer.StartArray();
    for (const FailedImgEntry& entry : unique)
        writer.String(entry.img.c_str());
    writer.EndArray();
    writer.Key("failures");
    writer.StartArray();
    for (const FailedImgEntry& entry : unique)
    {
        writer.StartObject();
        writer.Key("img");
        writer.String(entry.img.c_str());
        writer.Key("error");
        writer.String(entry.error.c_str());
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        error = "Failed to open failed list for writing: " + path.generic_string();
        return false;
    }
    file << buffer.GetString();
    if (!file)
    {
        error = "Failed to write failed list: " + path.generic_string();
        return false;
    }
    return true;
}

bool resolveTexture(const npk::Album& album,
                    std::size_t frameIndex,
                    const npk::TextureInfo*& resolved,
                    std::string& error)
{
    if (frameIndex >= album.textures.size())
    {
        error = "frame index is out of range";
        return false;
    }

    std::set<std::size_t> visited;
    std::size_t current = frameIndex;
    for (;;)
    {
        if (!visited.insert(current).second)
        {
            error = "linkIndex cycle detected";
            return false;
        }
        const auto& texture = album.textures[current];
        if (texture.type != npk::LINK)
        {
            resolved = &texture;
            return true;
        }
        if (texture.linkIndex < 0 || texture.linkIndex >= static_cast<int32_t>(album.textures.size()))
        {
            error = "linkIndex is out of range";
            return false;
        }
        current = static_cast<std::size_t>(texture.linkIndex);
    }
}

std::vector<std::filesystem::path> listNpkFiles(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> result;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error)
            break;
        if (!entry.is_regular_file(error))
            continue;
        if (lowerAscii(entry.path().extension().generic_string()) == ".npk")
            result.push_back(entry.path().lexically_normal());
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.generic_string() < rhs.generic_string();
    });
    return result;
}

std::string formatFrameError(std::size_t frameIndex, const std::string& message)
{
    return "frame " + std::to_string(frameIndex) + ": " + message;
}

}  // namespace

NpkExporterService::~NpkExporterService()
{
    shutdown();
}

bool NpkExporterService::start(NpkExportRequest request)
{
    if (isRunning())
        return false;

    if (m_worker.joinable())
        m_worker.join();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingLogs.clear();
        m_progress = {};
        m_progress.running = true;
        m_progress.phase = "Starting...";
    }
    m_cancelRequested.store(false);
    if (request.scanFailedOnly)
        m_worker = std::thread(&NpkExporterService::runScanFailed, this, std::move(request));
    else
        m_worker = std::thread(&NpkExporterService::run, this, std::move(request));
    return true;
}

void NpkExporterService::cancel()
{
    m_cancelRequested.store(true);
}

void NpkExporterService::shutdown()
{
    cancel();
    if (m_worker.joinable())
        m_worker.join();
}

bool NpkExporterService::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress.running;
}

NpkExportProgress NpkExporterService::progress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

void NpkExporterService::drainLogs(std::vector<NpkExportLogEntry>& entries)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    entries.insert(entries.end(), std::make_move_iterator(m_pendingLogs.begin()),
                   std::make_move_iterator(m_pendingLogs.end()));
    m_pendingLogs.clear();
}

void NpkExporterService::pushLog(NpkExportLogLevel level, std::string text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingLogs.push_back({level, std::move(text)});
}

void NpkExporterService::setPhase(std::string phase)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress.phase = std::move(phase);
}

void NpkExporterService::setFrameCounts(std::size_t completed, std::size_t total)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress.completedFrames = completed;
    m_progress.totalFrames = total;
}

void NpkExporterService::finish(bool success)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress.running = false;
    m_progress.finished = true;
    m_progress.success = success;
    m_progress.phase = m_cancelRequested.load() ? "Cancelled" : (success ? "Completed" : "Completed with errors");
}

bool NpkExporterService::mapImgFramePath(const std::filesystem::path& outputRoot,
                                         std::string_view imgPath,
                                         std::size_t frameIndex,
                                         std::filesystem::path& outputPath,
                                         std::string& error)
{
    std::string normalized = normalizeSlashes(std::string(imgPath));
    if (normalized.empty() || normalized.front() == '/' ||
        (normalized.size() > 1 && normalized[1] == ':'))
    {
        error = "IMG path must be relative";
        return false;
    }

    std::filesystem::path relative(normalized);
    for (const auto& component : relative)
    {
        if (component == "..")
        {
            error = "IMG path contains a parent-directory component";
            return false;
        }
    }

    const std::string extension = lowerAscii(relative.extension().generic_string());
    if (extension != ".img")
    {
        error = "IMG path does not have a .img extension";
        return false;
    }

    std::ostringstream frameName;
    frameName << std::setw(4) << std::setfill('0') << frameIndex << ".png";
    outputPath = outputRoot / relative.parent_path() / relative.stem() / frameName.str();
    return true;
}

bool NpkExporterService::validateTexture(const npk::TextureInfo& texture, std::string& error)
{
    // Empty bitmaps (width/height == 0) are valid transparent placeholders and must export.
    const int32_t canvasWidth  = std::max(1, texture.canvasWidth);
    const int32_t canvasHeight = std::max(1, texture.canvasHeight);
    const std::size_t canvasPixels = static_cast<std::size_t>(canvasWidth) * static_cast<std::size_t>(canvasHeight);
    if (canvasPixels > std::numeric_limits<std::size_t>::max() / 4)
    {
        error = "texture/canvas buffer is too large";
        return false;
    }

    if (texture.width <= 0 || texture.height <= 0)
        return true;

    const std::size_t sourcePixels = static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    if (sourcePixels > std::numeric_limits<std::size_t>::max() / 4 || texture.data.size() < sourcePixels * 4)
    {
        error = "texture buffer is too large or truncated";
        return false;
    }
    return true;
}

bool NpkExporterService::bakeCanvas(const npk::TextureInfo& texture,
                                    std::vector<uint8_t>& canvas,
                                    int32_t& canvasWidth,
                                    int32_t& canvasHeight,
                                    std::string& error)
{
    if (!validateTexture(texture, error))
        return false;

    canvasWidth  = std::max(1, texture.canvasWidth);
    canvasHeight = std::max(1, texture.canvasHeight);
    const std::size_t canvasPixels = static_cast<std::size_t>(canvasWidth) * static_cast<std::size_t>(canvasHeight);
    canvas.assign(canvasPixels * 4, 0);

    if (texture.width <= 0 || texture.height <= 0 || texture.data.empty())
        return true;

    for (int32_t y = 0; y < texture.height; ++y)
    {
        for (int32_t x = 0; x < texture.width; ++x)
        {
            const int32_t dstX = x + texture.x;
            const int32_t dstY = y + texture.y;
            if (dstX < 0 || dstY < 0 || dstX >= canvasWidth || dstY >= canvasHeight)
                continue;

            const std::size_t sourceOffset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(texture.width) + static_cast<std::size_t>(x)) *
                4;
            const std::size_t targetOffset =
                (static_cast<std::size_t>(dstY) * static_cast<std::size_t>(canvasWidth) + static_cast<std::size_t>(dstX)) *
                4;
            std::copy_n(texture.data.data() + sourceOffset, 4, canvas.data() + targetOffset);
        }
    }
    return true;
}

bool NpkExporterService::writeVFrame(const std::filesystem::path& path,
                                     const npk::TextureInfo& texture,
                                     std::string& error)
{
    const int32_t canvasWidth  = std::max(1, texture.canvasWidth);
    const int32_t canvasHeight = std::max(1, texture.canvasHeight);

    int32_t width   = 0;
    int32_t height  = 0;
    int32_t offsetX = 0;
    int32_t offsetY = 0;
    uint32_t dataSize = 0;
    const uint8_t* rgba = nullptr;

    if (texture.width > 0 && texture.height > 0 && !texture.data.empty())
    {
        const std::size_t expected =
            static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4;
        if (texture.data.size() < expected)
        {
            error = "texture buffer truncated for vframe";
            return false;
        }
        if (expected > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()))
        {
            error = "vframe pixel buffer is too large";
            return false;
        }
        width     = texture.width;
        height    = texture.height;
        offsetX   = texture.x;
        offsetY   = texture.y;
        dataSize  = static_cast<uint32_t>(expected);
        rgba      = texture.data.data();
    }

    yasio::obstream obs;
    obs.write_bytes("VF01", 4);
    obs.write<int32_t>(canvasWidth);
    obs.write<int32_t>(canvasHeight);
    obs.write<int32_t>(offsetX);
    obs.write<int32_t>(offsetY);
    obs.write<int32_t>(width);
    obs.write<int32_t>(height);
    obs.write<uint32_t>(dataSize);
    if (dataSize > 0 && rgba)
        obs.write_bytes(rgba, static_cast<int>(dataSize));

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "failed to open vframe for writing";
        return false;
    }
    if (!obs.empty())
        out.write(obs.data(), static_cast<std::streamsize>(obs.length()));
    if (!out)
    {
        error = "failed to write vframe";
        return false;
    }
    return true;
}

void NpkExporterService::runScanFailed(NpkExportRequest request)
{
    bool success = true;
    try
    {
        std::error_code error;
        if (!std::filesystem::is_directory(request.npkDirectory, error))
        {
            pushLog(NpkExportLogLevel::Error, "NPK directory does not exist: " + request.npkDirectory.generic_string());
            finish(false);
            return;
        }
        if (request.failedListPath.empty())
        {
            pushLog(NpkExportLogLevel::Error, "Failed-list path is empty.");
            finish(false);
            return;
        }

        const auto npkFiles = listNpkFiles(request.npkDirectory);
        if (npkFiles.empty())
        {
            pushLog(NpkExportLogLevel::Error, "No .npk files found in: " + request.npkDirectory.generic_string());
            finish(false);
            return;
        }

        setPhase("Counting IMGs...");
        std::size_t totalImgs = 0;
        for (const auto& npkPath : npkFiles)
        {
            if (m_cancelRequested.load())
            {
                finish(false);
                return;
            }
            npk::Npk npkFile;
            if (!npkFile.load(npkPath))
            {
                pushLog(NpkExportLogLevel::Error, npkPath.generic_string() + ": " + npkFile.lastError());
                success = false;
                continue;
            }
            totalImgs += npkFile.getIndexList().size();
        }
        setFrameCounts(0, totalImgs);
        pushLog(NpkExportLogLevel::Info, "Scanning " + std::to_string(totalImgs) + " IMGs for export failures...");

        setPhase("Scanning IMGs...");
        std::vector<FailedImgEntry> failedImgs;
        std::size_t completedImgs = 0;
        for (const auto& npkPath : npkFiles)
        {
            if (m_cancelRequested.load())
                break;

            npk::Npk npkFile;
            if (!npkFile.load(npkPath))
            {
                success = false;
                continue;
            }

            for (auto& index : npkFile.getIndexList())
            {
                if (m_cancelRequested.load())
                    break;

                std::string imageMessage;
                bool imageOk = true;
                if (!npkFile.loadTexturesFromImgFile(index))
                {
                    imageOk = false;
                    imageMessage = npkFile.lastError();
                }
                else if (!index.album)
                {
                    imageOk = false;
                    imageMessage = "IMG album is missing";
                }
                else
                {
                    for (std::size_t frameIndex = 0; frameIndex < index.album->textures.size(); ++frameIndex)
                    {
                        const npk::TextureInfo* texture = nullptr;
                        std::string frameError;
                        if (!resolveTexture(*index.album, frameIndex, texture, frameError))
                        {
                            imageOk = false;
                            imageMessage = formatFrameError(frameIndex, frameError);
                            break;
                        }
                        if (!validateTexture(*texture, frameError))
                        {
                            imageOk = false;
                            imageMessage = formatFrameError(frameIndex, frameError);
                            break;
                        }
                    }
                }

                if (!imageOk)
                {
                    failedImgs.push_back({index.name, imageMessage});
                    pushLog(NpkExportLogLevel::Warning, "FAIL " + index.name + ": " + imageMessage);
                }

                ++completedImgs;
                setFrameCounts(completedImgs, totalImgs);
            }
        }

        if (m_cancelRequested.load())
        {
            pushLog(NpkExportLogLevel::Warning, "Scan cancelled by user.");
            finish(false);
            return;
        }

        std::string writeError;
        std::size_t mergedCount = 0;
        std::size_t addedCount = 0;
        if (!writeFailedListFile(request.failedListPath, failedImgs, false, mergedCount, addedCount, writeError))
        {
            pushLog(NpkExportLogLevel::Error, writeError);
            finish(false);
            return;
        }

        pushLog(NpkExportLogLevel::Info,
                "Scan complete. Failed IMGs: " + std::to_string(failedImgs.size()) +
                    " (overwrote " + request.failedListPath.generic_string() + ")");
        finish(success);
    }
    catch (const std::exception& exception)
    {
        pushLog(NpkExportLogLevel::Error, std::string("Unhandled scan error: ") + exception.what());
        finish(false);
    }
    catch (...)
    {
        pushLog(NpkExportLogLevel::Error, "Unhandled scan error.");
        finish(false);
    }
}

void NpkExporterService::run(NpkExportRequest request)
{
    bool success = true;
    try
    {
        std::error_code error;
        if (!std::filesystem::is_directory(request.npkDirectory, error))
        {
            pushLog(NpkExportLogLevel::Error, "NPK directory does not exist: " + request.npkDirectory.generic_string());
            finish(false);
            return;
        }
        if (request.outputRoot.empty())
        {
            pushLog(NpkExportLogLevel::Error, "Output root is empty.");
            finish(false);
            return;
        }

        const auto npkFiles = listNpkFiles(request.npkDirectory);
        if (npkFiles.empty())
        {
            pushLog(NpkExportLogLevel::Error, "No .npk files found in: " + request.npkDirectory.generic_string());
            finish(false);
            return;
        }

        std::unordered_set<std::string> whitelist;
        if (!request.whitelistPath.empty())
        {
            std::string whitelistError;
            if (!loadWhitelistFile(request.whitelistPath, whitelist, whitelistError))
            {
                pushLog(NpkExportLogLevel::Error, whitelistError);
                finish(false);
                return;
            }
            pushLog(NpkExportLogLevel::Info,
                    "Loaded whitelist (" + std::to_string(whitelist.size()) +
                        " imgs): " + request.whitelistPath.generic_string());
        }

        setPhase("Scanning IMG headers...");
        std::size_t totalFrames = 0;
        bool selectedImageFound = false;
        std::unordered_set<std::string> hitWhitelist;
        for (const auto& npkPath : npkFiles)
        {
            if (m_cancelRequested.load())
            {
                finish(false);
                return;
            }

            npk::Npk npkFile;
            if (!npkFile.load(npkPath))
            {
                pushLog(NpkExportLogLevel::Error, npkPath.generic_string() + ": " + npkFile.lastError());
                success = false;
                continue;
            }
            for (auto& index : npkFile.getIndexList())
            {
                if (!passesImgSelection(index.name, request.imgFilter, whitelist))
                    continue;
                selectedImageFound = true;
                if (!whitelist.empty())
                    hitWhitelist.insert(index.name);
                int32_t count = 0;
                if (!npkFile.readTextureCount(index, count))
                {
                    pushLog(NpkExportLogLevel::Error,
                            npkPath.generic_string() + " / " + index.name + ": " + npkFile.lastError());
                    success = false;
                    continue;
                }
                totalFrames += static_cast<std::size_t>(count);
            }
        }
        setFrameCounts(0, totalFrames);

        if (!whitelist.empty())
        {
            pushLog(NpkExportLogLevel::Info,
                    "Whitelist hits in NPK indexes: " + std::to_string(hitWhitelist.size()) + " / " +
                        std::to_string(whitelist.size()));
            std::vector<std::string> missing;
            missing.reserve(whitelist.size());
            for (const std::string& img : whitelist)
            {
                if (hitWhitelist.find(img) == hitWhitelist.end())
                    missing.push_back(img);
            }
            std::sort(missing.begin(), missing.end());
            if (!missing.empty())
            {
                pushLog(NpkExportLogLevel::Warning,
                        "Whitelist entries not found in any NPK: " + std::to_string(missing.size()));
                constexpr std::size_t kMaxMissingLogs = 30;
                for (std::size_t i = 0; i < missing.size() && i < kMaxMissingLogs; ++i)
                    pushLog(NpkExportLogLevel::Warning, "Missing IMG: " + missing[i]);
                if (missing.size() > kMaxMissingLogs)
                    pushLog(NpkExportLogLevel::Warning,
                            "... and " + std::to_string(missing.size() - kMaxMissingLogs) + " more missing");
            }
        }

        if (!selectedImageFound)
        {
            pushLog(NpkExportLogLevel::Warning, "No IMG files matched the current filter/whitelist.");
            finish(success);
            return;
        }

        setPhase("Exporting frames...");
        std::set<std::filesystem::path> writtenOutputs;
        std::vector<FailedImgEntry> failedImgs;
        std::size_t completedFrames = 0;
        std::size_t skippedExistingFrames = 0;
        for (const auto& npkPath : npkFiles)
        {
            if (m_cancelRequested.load())
                break;

            npk::Npk npkFile;
            if (!npkFile.load(npkPath))
            {
                success = false;
                continue;
            }

            for (auto& index : npkFile.getIndexList())
            {
                if (m_cancelRequested.load())
                    break;
                if (!passesImgSelection(index.name, request.imgFilter, whitelist))
                    continue;

                bool imageSuccess = true;
                std::string imageMessage;

                if (!request.overwriteExisting)
                {
                    bool allFramesExist = true;
                    std::size_t presumedFrameCount = 0;
                    // Cheap existence probe using index metadata when possible; fall back after load.
                    // We don't know frame count until album is loaded, so probe after path mapping
                    // is deferred to per-frame skip below. For a faster whole-IMG skip, load count only.
                    int32_t textureCount = 0;
                    if (npkFile.readTextureCount(index, textureCount) && textureCount > 0)
                    {
                        presumedFrameCount = static_cast<std::size_t>(textureCount);
                        for (std::size_t frameIndex = 0; frameIndex < presumedFrameCount; ++frameIndex)
                        {
                            std::filesystem::path outputPath;
                            std::string pathError;
                            if (!NpkExporterService::mapImgFramePath(request.outputRoot, index.name, frameIndex,
                                                                     outputPath, pathError))
                            {
                                allFramesExist = false;
                                break;
                            }
                            outputPath = outputPath.lexically_normal();
                            const std::filesystem::path vfPath =
                                std::filesystem::path(outputPath).replace_extension(".vf");
                            std::error_code existsError;
                            if (!std::filesystem::is_regular_file(outputPath, existsError) ||
                                !std::filesystem::is_regular_file(vfPath, existsError))
                            {
                                allFramesExist = false;
                                break;
                            }
                        }
                        if (allFramesExist)
                        {
                            completedFrames += presumedFrameCount;
                            skippedExistingFrames += presumedFrameCount;
                            setFrameCounts(completedFrames, totalFrames);
                            pushLog(NpkExportLogLevel::Info,
                                    "SKIP " + index.name + " (" + std::to_string(presumedFrameCount) +
                                        " existing PNG+.vf)");
                            continue;
                        }
                    }
                }

                if (!npkFile.loadTexturesFromImgFile(index))
                {
                    imageSuccess = false;
                    imageMessage = npkFile.lastError();
                }
                else if (!index.album)
                {
                    imageSuccess = false;
                    imageMessage = "IMG album is missing";
                }
                else
                {
                    for (std::size_t frameIndex = 0; frameIndex < index.album->textures.size(); ++frameIndex)
                    {
                        const npk::TextureInfo* texture = nullptr;
                        std::string frameError;
                        if (!resolveTexture(*index.album, frameIndex, texture, frameError))
                        {
                            imageSuccess = false;
                            if (imageMessage.empty())
                                imageMessage = formatFrameError(frameIndex, frameError);
                            ++completedFrames;
                            setFrameCounts(completedFrames, totalFrames);
                            continue;
                        }

                        std::filesystem::path pngPath;
                        if (!mapImgFramePath(request.outputRoot, index.name, frameIndex, pngPath, frameError))
                        {
                            imageSuccess = false;
                            if (imageMessage.empty())
                                imageMessage = formatFrameError(frameIndex, frameError);
                            ++completedFrames;
                            setFrameCounts(completedFrames, totalFrames);
                            continue;
                        }
                        pngPath = pngPath.lexically_normal();
                        const std::filesystem::path vfPath =
                            std::filesystem::path(pngPath).replace_extension(".vf");

                        std::error_code existsError;
                        const bool pngExists = std::filesystem::is_regular_file(pngPath, existsError);
                        const bool vfExists  = std::filesystem::is_regular_file(vfPath, existsError);
                        const bool needPng   = request.overwriteExisting || !pngExists;
                        const bool needVf    = request.overwriteExisting || !vfExists;

                        if (!needPng && !needVf)
                        {
                            ++completedFrames;
                            ++skippedExistingFrames;
                            setFrameCounts(completedFrames, totalFrames);
                            continue;
                        }

                        std::string warning;
                        if (needPng && !writtenOutputs.insert(pngPath).second)
                            warning = "output path already produced; later NPK overwrites it";
                        if (needVf && !writtenOutputs.insert(vfPath).second && warning.empty())
                            warning = "vframe path already produced; later NPK overwrites it";

                        std::error_code directoryError;
                        std::filesystem::create_directories(pngPath.parent_path(), directoryError);
                        if (directoryError)
                        {
                            imageSuccess = false;
                            if (imageMessage.empty())
                                imageMessage = formatFrameError(frameIndex, "failed to create output directory");
                            ++completedFrames;
                            setFrameCounts(completedFrames, totalFrames);
                            continue;
                        }

                        if (needPng)
                        {
                            std::vector<uint8_t> canvas;
                            int32_t canvasWidth  = 0;
                            int32_t canvasHeight = 0;
                            if (!bakeCanvas(*texture, canvas, canvasWidth, canvasHeight, frameError))
                            {
                                imageSuccess = false;
                                if (imageMessage.empty())
                                    imageMessage = formatFrameError(frameIndex, frameError);
                                ++completedFrames;
                                setFrameCounts(completedFrames, totalFrames);
                                continue;
                            }

                            ax::Image image;
                            if (!image.initWithRawData(canvas.data(), static_cast<ssize_t>(canvas.size()), canvasWidth,
                                                       canvasHeight, 8, false) ||
                                !image.saveToFile(pngPath.generic_string(), false))
                            {
                                imageSuccess = false;
                                if (imageMessage.empty())
                                    imageMessage = formatFrameError(frameIndex, "failed to write PNG");
                                ++completedFrames;
                                setFrameCounts(completedFrames, totalFrames);
                                continue;
                            }
                        }

                        if (needVf)
                        {
                            if (!writeVFrame(vfPath, *texture, frameError))
                            {
                                imageSuccess = false;
                                if (imageMessage.empty())
                                    imageMessage = formatFrameError(frameIndex, frameError);
                                ++completedFrames;
                                setFrameCounts(completedFrames, totalFrames);
                                continue;
                            }
                        }

                        ++completedFrames;
                        setFrameCounts(completedFrames, totalFrames);
                        if (!warning.empty() && imageMessage.empty())
                            imageMessage = warning;
                    }
                }

                if (imageSuccess)
                {
                    std::string status =
                        "OK " + index.name + " (" + std::to_string(index.album ? index.album->textures.size() : 0) +
                        " frames)";
                    if (!imageMessage.empty())
                    {
                        pushLog(NpkExportLogLevel::Warning, status + " - " + imageMessage);
                    }
                    else
                    {
                        pushLog(NpkExportLogLevel::Info, std::move(status));
                    }
                }
                else
                {
                    success = false;
                    failedImgs.push_back({index.name, imageMessage});
                    pushLog(NpkExportLogLevel::Error, "ERROR " + index.name + ": " + imageMessage);
                }
            }
        }

        if (!request.overwriteExisting && skippedExistingFrames > 0)
        {
            pushLog(NpkExportLogLevel::Info,
                    "Skipped existing PNG+.vf frames (overwrite disabled): " +
                        std::to_string(skippedExistingFrames));
        }

        if (!request.failedListPath.empty())
        {
            std::string writeError;
            std::size_t mergedCount = 0;
            std::size_t addedCount = 0;
            if (!writeFailedListFile(request.failedListPath, failedImgs, request.mergeFailedList, mergedCount,
                                     addedCount, writeError))
            {
                pushLog(NpkExportLogLevel::Error, writeError);
                success = false;
            }
            else if (request.mergeFailedList)
            {
                pushLog(NpkExportLogLevel::Info,
                        "Merged failed IMG list (added " + std::to_string(addedCount) + ", total " +
                            std::to_string(mergedCount) + ", this run " + std::to_string(failedImgs.size()) +
                            "): " + request.failedListPath.generic_string());
            }
            else
            {
                pushLog(NpkExportLogLevel::Info,
                        "Wrote failed IMG list (" + std::to_string(mergedCount) +
                            "): " + request.failedListPath.generic_string());
            }
        }

        if (m_cancelRequested.load())
        {
            pushLog(NpkExportLogLevel::Warning, "Export cancelled by user.");
            finish(false);
        }
        else
        {
            finish(success);
        }
    }
    catch (const std::exception& exception)
    {
        pushLog(NpkExportLogLevel::Error, std::string("Unhandled exporter error: ") + exception.what());
        finish(false);
    }
    catch (...)
    {
        pushLog(NpkExportLogLevel::Error, "Unhandled exporter error.");
        finish(false);
    }
}

}  // namespace editor
