#include "fimlite/paths.hpp"

#include <cstddef>
#include <cstdlib>
#include <memory>

namespace fimlite
{

std::string to_utf8(const std::filesystem::path& path)
{
    return path.generic_u8string();
}

std::string to_utf8_native(const std::filesystem::path& path)
{
    return path.u8string();
}

std::filesystem::path from_utf8(const std::string& text)
{
    return std::filesystem::u8path(text);
}

std::string utf8_env(const std::string& name)
{
#ifdef _WIN32
    wchar_t* buffer = nullptr;
    std::size_t length = 0;

    if (_wdupenv_s(&buffer, &length, from_utf8(name).c_str()) != 0 || !buffer)
    {
        return std::string{};
    }

    const std::unique_ptr<wchar_t, decltype(&std::free)> owned(buffer, &std::free);

    return to_utf8_native(std::filesystem::path(owned.get()));
#else
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : std::string{};
#endif
}

#ifdef _WIN32
std::vector<std::string> utf8_args(int argc, const wchar_t* const* argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));

    for (int i = 0; i < argc; ++i)
    {
        args.push_back(to_utf8_native(std::filesystem::path(argv[i])));
    }

    return args;
}
#endif

} // namespace fimlite
