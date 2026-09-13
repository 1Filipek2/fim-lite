#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fimlite
{

std::string to_utf8(const std::filesystem::path& path);
std::string to_utf8_native(const std::filesystem::path& path);
std::filesystem::path from_utf8(const std::string& text);
std::string utf8_env(const std::string& name);

#ifdef _WIN32
std::vector<std::string> utf8_args(int argc, const wchar_t* const* argv);
#endif

} // namespace fimlite