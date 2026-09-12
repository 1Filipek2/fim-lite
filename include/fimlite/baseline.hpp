#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace fimlite
{

struct FileRecord
{
    std::string path;
    std::string hash;
    std::uintmax_t size = 0;
    long long mtime = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileRecord, path, hash, size, mtime);
};

using FileRecordMap = std::map<std::string, FileRecord>;

void save_baseline(const FileRecordMap& records,
                   const std::filesystem::path& out,
                   const std::vector<std::string>& exclude_names = {});

FileRecordMap load_baseline(const std::filesystem::path& in,
                            std::vector<std::string>* exclude_names_out = nullptr);

} // namespace fimlite
