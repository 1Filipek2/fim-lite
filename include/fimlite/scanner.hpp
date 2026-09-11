#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fimlite/baseline.hpp"

namespace fimlite
{

enum class ChangeType
{
    Added,
    Removed,
    Modified
};

enum class SkipReason
{
    Error,      // could not be read: the run is incomplete
    Symlink     // intentionally not followed: the run is still complete
};

struct SkippedEntry
{
    std::string path;
    std::string reason;
    SkipReason kind = SkipReason::Error;
};

struct Change
{
    ChangeType type;
    std::string path;
};

FileRecordMap scan_directory(const std::filesystem::path& root,
                             const std::vector<std::string>& exclude_names = {},
                             std::vector<SkippedEntry>* skipped = nullptr);
std::vector<Change> diff(const FileRecordMap& baseline, const FileRecordMap& current);

} // namespace fimlite