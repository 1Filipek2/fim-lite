#include "fimlite/baseline.hpp"
#include "fimlite/hasher.hpp"
#include "fimlite/scanner.hpp"

#include <chrono>
#include <iostream>
#include <filesystem>
#include <stdexcept>

namespace fimlite
{

FileRecordMap scan_directory(const std::filesystem::path& root)
{
    if (!std::filesystem::exists(root))
    {
        throw std::runtime_error("Root directory does not exist: " + root.string());
    }
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("Path is not a directory: " + root.string());
    }

    FileRecordMap result;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        try
        {
            const std::string path = std::filesystem::relative(entry.path(), root).string();
            const auto size = entry.file_size();
            const auto last_write_time = entry.last_write_time();

            const auto file_now = std::filesystem::file_time_type::clock::now();
            const auto sys_now = std::chrono::system_clock::now();

            const auto sys_time_point = last_write_time - file_now + sys_now;
            const auto casted_time_point = std::chrono::time_point_cast<std::chrono::seconds>(sys_time_point);

            const long long epoch_time = static_cast<long long>(std::chrono::system_clock::to_time_t(casted_time_point));

            const auto hash = sha256_file(entry.path());

            result[path] = FileRecord{path, hash, size, epoch_time};
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "Error processing " << entry.path() << ": " << e.what() << '\n';
        }
    }

    return result;
}

std::vector<Change> diff(const FileRecordMap& baseline, const FileRecordMap& current)
{
    std::vector<Change> result;

    auto baseline_it = baseline.begin();
    auto current_it = current.begin();

    while (baseline_it != baseline.end() && current_it != current.end())
    {
        if (baseline_it->first < current_it->first)
        {
            result.push_back({ChangeType::Removed, baseline_it->first});
            ++baseline_it;
        }
        else if (current_it->first < baseline_it->first)
        {
            result.push_back({ChangeType::Added, current_it->first});
            ++current_it;
        }
        else
        {
            if (baseline_it->second.hash != current_it->second.hash)
            {
                result.push_back({ChangeType::Modified, current_it->first});
            }

            ++baseline_it;
            ++current_it;
        }
    }

    while (baseline_it != baseline.end())
    {
        result.push_back({ChangeType::Removed, baseline_it->first});
        ++baseline_it;
    }

    while (current_it != current.end())
    {
        result.push_back({ChangeType::Added, current_it->first});
        ++current_it;
    }

    return result;
}

} // namespace fimlite