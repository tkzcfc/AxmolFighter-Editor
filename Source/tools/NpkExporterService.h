#pragma once

#include "npk/Npk.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace editor
{

enum class NpkExportLogLevel
{
    Info,
    Warning,
    Error,
};

struct NpkExportRequest
{
    std::filesystem::path npkDirectory;
    std::filesystem::path outputRoot;
    std::string imgFilter;
    std::filesystem::path whitelistPath;
    // When set, write failed IMG paths here as JSON after export/scan.
    std::filesystem::path failedListPath;
    // When false (default), skip writing a PNG/.vf that already exists (checked separately).
    bool overwriteExisting = false;
    // When true, only validate all IMGs and overwrite failedListPath (no PNG export).
    bool scanFailedOnly = false;
    // When true (default for export), merge into existing failed list; scan uses false.
    bool mergeFailedList = true;
};

struct NpkExportProgress
{
    std::size_t completedFrames = 0;
    std::size_t totalFrames     = 0;
    bool running                = false;
    bool finished               = false;
    bool success                = false;
    std::string phase;
};

struct NpkExportLogEntry
{
    NpkExportLogLevel level = NpkExportLogLevel::Info;
    std::string text;
};

class NpkExporterService final
{
public:
    NpkExporterService() = default;
    ~NpkExporterService();

    NpkExporterService(const NpkExporterService&) = delete;
    NpkExporterService& operator=(const NpkExporterService&) = delete;

    bool start(NpkExportRequest request);
    void cancel();
    void shutdown();
    bool isRunning() const;

    NpkExportProgress progress() const;
    void drainLogs(std::vector<NpkExportLogEntry>& entries);

    static bool mapImgFramePath(const std::filesystem::path& outputRoot,
                                std::string_view imgPath,
                                std::size_t frameIndex,
                                std::filesystem::path& outputPath,
                                std::string& error);
    static bool bakeCanvas(const npk::TextureInfo& texture,
                           std::vector<uint8_t>& canvas,
                           int32_t& canvasWidth,
                           int32_t& canvasHeight,
                           std::string& error);
    // Write cropped RGBA virtual-frame sidecar (same stem as PNG, .vf extension).
    static bool writeVFrame(const std::filesystem::path& path,
                            const npk::TextureInfo& texture,
                            std::string& error);
    // Dimension/buffer checks only (no pixel copy) for fast failure scanning.
    static bool validateTexture(const npk::TextureInfo& texture, std::string& error);

private:
    void run(NpkExportRequest request);
    void runScanFailed(NpkExportRequest request);
    void pushLog(NpkExportLogLevel level, std::string text);
    void setPhase(std::string phase);
    void setFrameCounts(std::size_t completed, std::size_t total);
    void finish(bool success);

    mutable std::mutex m_mutex;
    std::vector<NpkExportLogEntry> m_pendingLogs;
    NpkExportProgress m_progress;
    std::thread m_worker;
    std::atomic<bool> m_cancelRequested{false};
};

}  // namespace editor
