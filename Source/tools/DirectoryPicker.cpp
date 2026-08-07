#include "tools/DirectoryPicker.h"

#if defined(_WIN32)
#    include <objbase.h>
#    include <shlobj.h>
#    include <windows.h>
#endif

namespace editor
{

bool pickDirectory(std::filesystem::path& selectedDirectory)
{
#if defined(_WIN32)
    BROWSEINFOW browseInfo{};
    browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    browseInfo.lpszTitle = L"Select directory";

    PIDLIST_ABSOLUTE itemIdList = SHBrowseForFolderW(&browseInfo);
    if (itemIdList == nullptr)
        return false;

    wchar_t pathBuffer[MAX_PATH]{};
    const bool succeeded = SHGetPathFromIDListW(itemIdList, pathBuffer) != FALSE;
    CoTaskMemFree(itemIdList);
    if (!succeeded)
        return false;

    selectedDirectory = std::filesystem::path(pathBuffer).lexically_normal();
    return true;
#else
    (void)selectedDirectory;
    return false;
#endif
}

}  // namespace editor
