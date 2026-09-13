#include "fimlite/baseline.hpp"
#include "fimlite/fsync.hpp"
#include "fimlite/hmac.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fimlite
{

namespace
{

std::filesystem::path make_tmp_path(const std::filesystem::path& out)
{
    std::random_device device;
    std::uniform_int_distribution<std::uint64_t> distribution;

    std::ostringstream suffix;
    suffix << '.' << std::hex << std::setw(16) << std::setfill('0') << distribution(device) << ".tmp";

    std::filesystem::path tmp_path = out;
    tmp_path += suffix.str();

    return tmp_path;
}

void remove_tmp_file(const std::filesystem::path& tmp_path)
{
    std::error_code remove_ec;
    std::filesystem::remove(tmp_path, remove_ec);
}

} // namespace

void save_baseline(const FileRecordMap& records,
                   const std::filesystem::path& out,
                   const std::vector<std::string>& exclude_names,
                   const std::string& hmac_key)
{
    nlohmann::json j;
    j["version"] = 1;

    std::vector<FileRecord> files;

    for (const auto& [path, record] : records)
    {
        files.push_back(record);
    }

    j["files"] = files;
    j["exclude"] = exclude_names;

    if (!hmac_key.empty())
    {
        j["hmac"] = hmac_sha256_hex(hmac_key, j.dump());
    }

    const std::filesystem::path tmp_path = make_tmp_path(out);

    {
        std::ofstream file(tmp_path);

        if (!file)
        {
            throw std::runtime_error("Failed to open baseline file");
        }

        file << j.dump(4);
        file.flush();

        if (!file)
        {
            remove_tmp_file(tmp_path);
            throw std::runtime_error("Failed to write baseline file");
        }

        file.close();

        if (file.fail())
        {
            remove_tmp_file(tmp_path);
            throw std::runtime_error("Failed to close baseline file");
        }
    }

    try
    {
        sync_file(tmp_path);
    }
    catch (const std::exception&)
    {
        remove_tmp_file(tmp_path);
        throw;
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, out, ec);

    if (ec)
    {
        remove_tmp_file(tmp_path);
        throw std::runtime_error("Failed to finalize baseline file: " + ec.message());
    }

    sync_parent_directory(out);
}

FileRecordMap load_baseline(const std::filesystem::path& in,
                            std::vector<std::string>* exclude_names_out,
                            const std::string& hmac_key)
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

        const std::string stored_hmac = j.value("hmac", std::string{});
        j.erase("hmac");

        if (!hmac_key.empty())
        {
            if (stored_hmac.empty())
            {
                throw BaselineSignatureError("Baseline is not signed but an HMAC key was provided");
            }

            if (!hmac_hex_equal(hmac_sha256_hex(hmac_key, j.dump()), stored_hmac))
            {
                throw BaselineSignatureError("Baseline signature verification failed");
            }
        }
        else if (!stored_hmac.empty())
        {
            std::cerr << "Warning: baseline is signed but no HMAC key was provided; "
                         "signature was NOT verified.\n";
        }

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

        if (exclude_names_out)
        {
            *exclude_names_out = j.value("exclude", std::vector<std::string>{});
        }

        return records;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw std::runtime_error(std::string("Failed to parse baseline file: ") + e.what());
    }
}

} // namespace fimlite