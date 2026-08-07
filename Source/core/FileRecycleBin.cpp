#include "core/FileRecycleBin.h"

#include "platform/PlatformConfig.h"

#include <system_error>

#if AX_TARGET_PLATFORM == AX_PLATFORM_WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <shellapi.h>
#endif

namespace editor
{
namespace
{
bool deletePathPermanently(const std::filesystem::path& path, std::string& message)
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
    if (error)
    {
        message = "Failed to inspect path before deleting: " + error.message();
        return false;
    }
    if (!std::filesystem::exists(status))
    {
        message = "Path does not exist.";
        return false;
    }

    if (std::filesystem::is_directory(status))
    {
        std::filesystem::remove_all(path, error);
    }
    else
    {
        std::filesystem::remove(path, error);
    }

    if (error)
    {
        message = "Failed to delete path: " + error.message();
        return false;
    }
    return true;
}
}  // namespace

bool deletePathWithPlatformTrash(const std::filesystem::path& path, std::string& message)
{
    message.clear();
#if AX_TARGET_PLATFORM == AX_PLATFORM_WIN32
    std::wstring from = path.wstring();
    from.push_back(L'\0');
    from.push_back(L'\0');

    SHFILEOPSTRUCTW operation = {};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    const int result = SHFileOperationW(&operation);
    if (result != 0 || operation.fAnyOperationsAborted)
    {
        message = operation.fAnyOperationsAborted ? "Delete was canceled." :
                                                   "Failed to move file to recycle bin.";
        return false;
    }
    return true;
#else
    return deletePathPermanently(path, message);
#endif
}
}  // namespace editor
