#include "fimlite/fsync.hpp"
#include "fimlite/paths.hpp"

#include <string>
#include <system_error>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#else

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#endif

namespace fimlite
{

#ifdef _WIN32

void sync_file(const std::filesystem::path& path)
{
    const HANDLE handle = CreateFileW(path.c_str(),
                                      GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "Failed to open file for sync: " + to_utf8_native(path));
    }

    const BOOL flushed = FlushFileBuffers(handle);
    const DWORD flush_error = GetLastError();

    CloseHandle(handle);

    if (!flushed)
    {
        throw std::system_error(static_cast<int>(flush_error), std::system_category(),
                                "Failed to sync file: " + to_utf8_native(path));
    }
}

void sync_parent_directory(const std::filesystem::path&)
{
}

#else

namespace
{

void sync_descriptor(const std::filesystem::path& path,
                     int flags,
                     const std::string& description,
                     bool ignore_unsupported)
{
    const int descriptor = ::open(path.c_str(), flags);

    if (descriptor == -1)
    {
        throw std::system_error(errno, std::generic_category(),
                                "Failed to open " + description + " for sync: " + to_utf8_native(path));
    }

    const int result = ::fsync(descriptor);
    const int sync_error = errno;

    ::close(descriptor);

    if (result == 0)
    {
        return;
    }

    if (ignore_unsupported && (sync_error == EINVAL || sync_error == ENOTSUP))
    {
        return;
    }

    throw std::system_error(sync_error, std::generic_category(),
                            "Failed to sync " + description + ": " + to_utf8_native(path));
}

} // namespace

void sync_file(const std::filesystem::path& path)
{
    sync_descriptor(path, O_RDONLY, "file", false);
}

void sync_parent_directory(const std::filesystem::path& path)
{
    const std::filesystem::path parent = path.parent_path();

    sync_descriptor(parent.empty() ? std::filesystem::path(".") : parent,
                    O_RDONLY | O_DIRECTORY,
                    "directory",
                    true);
}

#endif

} // namespace fimlite
