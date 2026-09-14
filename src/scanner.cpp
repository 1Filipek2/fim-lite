#include "fimlite/baseline.hpp"
#include "fimlite/hasher.hpp"
#include "fimlite/scanner.hpp"
#include "fimlite/paths.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fimlite
{

namespace
{

bool equals_ignore_case(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    return std::equal(left.begin(), left.end(), right.begin(),
                      [](unsigned char left_char, unsigned char right_char)
                      {
                          return std::tolower(left_char) == std::tolower(right_char);
                      });
}

bool should_exclude(const std::filesystem::path& path,
                    const std::vector<std::string>& exclude_names)
{
    const std::string filename = to_utf8_native(path.filename());

    for (const auto& pattern : exclude_names)
    {
        if (equals_ignore_case(filename, pattern))
        {
            return true;
        }
    }

    return false;
}

std::string relative_key_or_empty(const std::filesystem::path& path, const std::filesystem::path& root)
{
    try
    {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(path, root, ec);

        return ec ? std::string{} : to_utf8(relative);
    }
    catch (const std::exception&)
    {
        return std::string{};
    }
}

std::runtime_error unreadable_root_error(const std::filesystem::path& root, const std::error_code& ec)
{
    return std::runtime_error("Cannot read root directory: " + to_utf8_native(root) + ": " + ec.message());
}

bool is_covered_by_scan_error(const std::string& path, const std::vector<SkippedEntry>& skipped)
{
    return std::any_of(skipped.begin(), skipped.end(),
                       [&path](const SkippedEntry& entry)
                       {
                           if (entry.kind != SkipReason::Error)
                           {
                               return false;
                           }

                           const std::string& prefix = entry.relative_path;

                           return prefix.empty() ||
                                  path == prefix ||
                                  (path.size() > prefix.size() &&
                                   path.compare(0, prefix.size(), prefix) == 0 &&
                                   path[prefix.size()] == '/');
                       });
}

} // namespace

bool exclude_names_equal_ignore_case(const std::vector<std::string>& left,
                                     const std::vector<std::string>& right)
{
    return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                      [](const std::string& left_name, const std::string& right_name)
                      {
                          return equals_ignore_case(left_name, right_name);
                      });
}

FileRecordMap scan_directory(const std::filesystem::path& root,
                             const std::vector<std::string>& exclude_names,
                             std::vector<SkippedEntry>* skipped)
{
    if (!std::filesystem::exists(root))
    {
        throw std::runtime_error("Root directory does not exist: " + to_utf8_native(root));
    }

    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("Path is not a directory: " + to_utf8_native(root));
    }

    FileRecordMap result;
    std::error_code ec;

    const std::filesystem::directory_iterator root_probe(root, ec);

    if (ec)
    {
        throw unreadable_root_error(root, ec);
    }

    const std::filesystem::recursive_directory_iterator end_iter;

    std::filesystem::recursive_directory_iterator dir_iter(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        ec);

    if (ec)
    {
        throw unreadable_root_error(root, ec);
    }

    while (dir_iter != end_iter)
    {
        std::string current_path;
        std::filesystem::path current_entry_path;

        try
        {
            const auto& entry = *dir_iter;
            current_entry_path = entry.path();
            current_path = to_utf8_native(entry.path());

            if (should_exclude(entry.path(), exclude_names))
            {
                if (entry.is_directory())
                {
                    dir_iter.disable_recursion_pending();
                }
            }
            else if (entry.is_symlink())
            {
                if (skipped)
                {
                    skipped->push_back({current_path, "Symlink not followed", SkipReason::Symlink});
                }
            }
            else if (entry.is_directory())
            {
                std::error_code probe_ec;
                const std::filesystem::directory_iterator probe(entry.path(), probe_ec);

                if (probe_ec)
                {
                    if (skipped)
                    {
                        skipped->push_back({current_path,
                                            probe_ec.message(),
                                            SkipReason::Error,
                                            relative_key_or_empty(entry.path(), root)});
                    }

                    dir_iter.disable_recursion_pending();
                }
            }
            else if (entry.is_regular_file())
            {
                const std::string relative_path = to_utf8(std::filesystem::relative(entry.path(), root));
                const auto size = entry.file_size();
                const auto last_write_time = entry.last_write_time();
                const auto file_now = std::filesystem::file_time_type::clock::now();
                const auto sys_now = std::chrono::system_clock::now();
                const auto sys_time_point = last_write_time - file_now + sys_now;
                const auto casted_time_point =
                    std::chrono::time_point_cast<std::chrono::seconds>(sys_time_point);
                const long long epoch_time =
                    static_cast<long long>(std::chrono::system_clock::to_time_t(casted_time_point));
                const auto hash = sha256_file(entry.path());

                result[relative_path] = FileRecord
                {
                    relative_path,
                    hash,
                    size,
                    epoch_time
                };
            }
        }
        catch (const std::exception& e)
        {
            if (skipped)
            {
                skipped->push_back({current_path.empty() ? "<unknown path>" : current_path,
                                    e.what(),
                                    SkipReason::Error,
                                    current_entry_path.empty()
                                        ? std::string{}
                                        : relative_key_or_empty(current_entry_path, root)});
            }
        }

        dir_iter.increment(ec);

        if (ec)
        {
            if (skipped)
            {
                skipped->push_back({current_path.empty() ? "<unknown path>" : current_path,
                                    "Traversal aborted: " + ec.message(),
                                    SkipReason::Error,
                                    std::string{}});
            }

            ec.clear();
            break;
        }
    }

    return result;
}

std::vector<Change> diff(const FileRecordMap& baseline,
                         const FileRecordMap& current,
                         const std::vector<SkippedEntry>& skipped)
{
    std::vector<Change> result;

    const auto report_removed = [&result, &skipped](const std::string& path)
    {
        if (!is_covered_by_scan_error(path, skipped))
        {
            result.push_back({ChangeType::Removed, path});
        }
    };

    auto baseline_it = baseline.begin();
    auto current_it = current.begin();

    while (baseline_it != baseline.end() && current_it != current.end())
    {
        if (baseline_it->first < current_it->first)
        {
            report_removed(baseline_it->first);
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
        report_removed(baseline_it->first);
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