#pragma once

#include <filesystem>

namespace fimlite
{

void sync_file(const std::filesystem::path& path);
void sync_parent_directory(const std::filesystem::path& path);

} // namespace fimlite
