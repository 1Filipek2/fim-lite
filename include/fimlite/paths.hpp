#pragma once

#include <filesystem>
#include <string>

namespace fimlite
{

std::string to_utf8(const std::filesystem::path& path);
std::string to_utf8_native(const std::filesystem::path& path);
std::filesystem::path from_utf8(const std::string& text); 

} // namespace fimlite