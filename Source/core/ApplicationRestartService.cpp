#include "core/ApplicationRestartService.h"

#include "base/Director.h"

#include <filesystem>
#include <string>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#    include <sys/types.h>
#    include <unistd.h>
#else
#    include <sys/types.h>
#    include <unistd.h>
#endif

namespace editor
{
namespace
{
#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (length <= 0)
        return {};

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
    result.resize(static_cast<std::size_t>(length - 1));
    return result;
}

std::string currentExecutablePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size())
    {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    if (length == 0)
        return {};

    buffer.resize(length);
    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, buffer.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0)
        return {};

    std::string result(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer.c_str(), -1, result.data(), utf8Length, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(utf8Length - 1));
    return result;
}

bool launchExecutable(const std::string& executablePath)
{
    const std::wstring executable = utf8ToWide(executablePath);
    if (executable.empty())
        return false;

    std::wstring commandLine = L"\"" + executable + L"\"";
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    const BOOL launched = CreateProcessW(executable.c_str(),
                                         commandLine.data(),
                                         nullptr,
                                         nullptr,
                                         FALSE,
                                         0,
                                         nullptr,
                                         nullptr,
                                         &startupInfo,
                                         &processInfo);
    if (!launched)
        return false;

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
#elif defined(__APPLE__)
std::string currentExecutablePath()
{
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0)
        return {};

    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        return {};
    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    return std::filesystem::weakly_canonical(buffer).generic_string();
}

bool launchExecutable(const std::string& executablePath)
{
    const pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0)
    {
        setsid();
        execl(executablePath.c_str(), executablePath.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    return true;
}
#else
std::string currentExecutablePath()
{
    std::string buffer(4096, '\0');
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0)
        return {};
    buffer.resize(static_cast<std::size_t>(length));
    return std::filesystem::weakly_canonical(buffer).generic_string();
}

bool launchExecutable(const std::string& executablePath)
{
    const pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0)
    {
        setsid();
        execl(executablePath.c_str(), executablePath.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    return true;
}
#endif
}  // namespace

bool ApplicationRestartService::restart(std::string* message) const
{
    if (message)
        message->clear();

    const std::string executable = currentExecutablePath();
    if (executable.empty())
    {
        if (message)
            *message = "Failed to resolve current executable path.";
        return false;
    }

    if (!launchExecutable(executable))
    {
        if (message)
            *message = "Failed to launch editor process.";
        return false;
    }

    ax::Director::getInstance()->end();
    return true;
}
}  // namespace editor
