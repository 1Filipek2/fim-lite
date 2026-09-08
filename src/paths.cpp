#include "fimlite/paths.hpp"

namespace fimlite
{

namespace 
{

template <typename U8String>
std::string as_bytes(const U8String& value)
{
    return std::string(value.begin(), value.end());
}

} // namespace

std::string to_utf8(const std::filesystem::path& path)
{
    return as_bytes(path.generic_u8string());
}

std::string to_utf8_native(const std::filesystem::path& path)
{
    return as_bytes(path.u8string());
}

std::filesystem::path from_utf8(const std::string& text)
{
#if defined(__cpp_lib_char8_t)
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
#else
    return std::filesystem::u8path(text);
#endif
}

} // namespace fimlite