#include "fimlite/paths.hpp"

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
