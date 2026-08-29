#include "fimlite/baseline.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fimlite
{

void save_baseline(const FileRecordMap& records, const std::filesystem::path& out)
{
    nlohmann::json j;
    j["version"] = 1;

    std::vector<FileRecord> files;

    for (const auto& [path, record] : records)
    {
        files.push_back(record);
    }

    j["files"] = files;

    std::ofstream file(out);

    if (!file)
    {
        throw std::runtime_error("Failed to open baseline file");
    }

    file << j.dump(4);
}

FileRecordMap load_baseline(const std::filesystem::path& in)
{
    if (!std::filesystem::exists(in))
    {
        throw std::runtime_error("Baseline file does not exist");
    }

    std::ifstream file(in);

    if (!file)
    {
        throw std::runtime_error("Failed to open baseline file");
    }

    nlohmann::json j;

    try
    {
        file >> j;

        const int version = j.at("version").get<int>();

        if (version != 1)
        {
            throw std::runtime_error("Unsupported baseline version");
        }

        const std::vector<FileRecord> files = j.at("files").get<std::vector<FileRecord>>();

        FileRecordMap records;

        for (const auto& record : files)
        {
            records[record.path] = record;
        }

        return records;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw std::runtime_error(std::string("Failed to parse baseline file: ") + e.what());
    }
}

} // namespace fimlite