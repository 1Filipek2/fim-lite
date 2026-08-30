#pragma once

#include "fimlite/baseline.hpp"

#include <filesystem>
#include <vector>
#include <string>

namespace fimlite
{

enum class ChangeType
{
    Added,
    Removed,
    Modified
};

struct Change
{
    ChangeType type;
    std::string path;
};

FileRecordMap scan_directory(const std::filesystem::path& root);
std::vector<Change> diff(const FileRecordMap& baseline, const FileRecordMap& current);

} // namespace fimlite