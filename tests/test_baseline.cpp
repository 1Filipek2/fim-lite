#include <catch2/catch_test_macros.hpp>

#include "fimlite/baseline.hpp"
#include "fimlite/paths.hpp"
#include "fimlite/scanner.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("Baseline can be saved and loaded")
{
    const std::filesystem::path baseline_path = "test_baseline.json";

    fimlite::FileRecordMap original_records;

    fimlite::FileRecord record
    {
        "test.txt",
        "abc123",
        1024,
        123456789
    };

    original_records[record.path] = record;

    fimlite::save_baseline(original_records, baseline_path);

    const auto loaded_records = fimlite::load_baseline(baseline_path);

    REQUIRE(loaded_records.size() == 1);
    REQUIRE(loaded_records.find("test.txt") != loaded_records.end());

    const auto& loaded = loaded_records.at("test.txt");

    REQUIRE(loaded.path == "test.txt");
    REQUIRE(loaded.hash == "abc123");
    REQUIRE(loaded.size == 1024);
    REQUIRE(loaded.mtime == 123456789);

    std::filesystem::remove(baseline_path);
}

TEST_CASE("Baseline round-trips non-ASCII paths and file names")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_unicode_baseline";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto baseline_path = temp_dir / fimlite::from_utf8(u8"záčklad.json");

    fimlite::FileRecordMap records;

    records[u8"podadresár/súbor_ľšť.txt"] = fimlite::FileRecord
    {
        u8"podadresár/súbor_ľšť.txt",
        "abc",
        3,
        0
    };

    fimlite::save_baseline(records, baseline_path);

    REQUIRE(std::filesystem::exists(baseline_path));

    const auto loaded = fimlite::load_baseline(baseline_path);

    REQUIRE(fimlite::diff(records, loaded).empty());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Baseline write is not blocked by a leftover fixed temp path")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_baseline_leftover_tmp";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto baseline_path = temp_dir / "baseline.json";

    std::filesystem::create_directories(temp_dir / "baseline.json.tmp");

    fimlite::FileRecordMap records;

    records["kept.txt"] = fimlite::FileRecord
    {
        "kept.txt",
        "abc",
        3,
        0
    };

    fimlite::save_baseline(records, baseline_path);

    REQUIRE(fimlite::diff(records, fimlite::load_baseline(baseline_path)).empty());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Failed baseline finalize leaves no temp file behind")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_baseline_finalize_failure";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto baseline_path = temp_dir / "baseline.json";

    std::filesystem::create_directories(baseline_path);
    std::ofstream(baseline_path / "occupied.txt") << "occupied";

    fimlite::FileRecordMap records;

    records["kept.txt"] = fimlite::FileRecord
    {
        "kept.txt",
        "abc",
        3,
        0
    };

    REQUIRE_THROWS_AS(fimlite::save_baseline(records, baseline_path), std::runtime_error);

    std::size_t entry_count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(temp_dir))
    {
        REQUIRE(entry.path() == baseline_path);
        ++entry_count;
    }

    REQUIRE(entry_count == 1);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Baseline round-trips exclude patterns")
{
    const std::filesystem::path baseline_path = "test_baseline_exclude.json";

    fimlite::FileRecordMap records;

    records["kept.txt"] = fimlite::FileRecord
    {
        "kept.txt",
        "abc",
        3,
        0
    };

    const std::vector<std::string> exclude_names =
    {
        "node_modules",
        ".git",
        u8"zálohy"
    };

    fimlite::save_baseline(records, baseline_path, exclude_names);

    std::vector<std::string> loaded_exclude_names;

    const auto loaded_records = fimlite::load_baseline(baseline_path, &loaded_exclude_names);

    REQUIRE(loaded_records.size() == 1);
    REQUIRE(loaded_exclude_names == exclude_names);

    std::filesystem::remove(baseline_path);
}

TEST_CASE("Baseline without an exclude key still loads")
{
    const std::filesystem::path baseline_path = "test_baseline_legacy.json";

    {
        std::ofstream file(baseline_path);

        file << R"({"version":1,"files":[{"path":"kept.txt","hash":"abc","size":3,"mtime":0}]})";
    }

    std::vector<std::string> loaded_exclude_names = {"stale"};

    const auto loaded_records = fimlite::load_baseline(baseline_path, &loaded_exclude_names);

    REQUIRE(loaded_records.size() == 1);
    REQUIRE(loaded_exclude_names.empty());

    std::filesystem::remove(baseline_path);
}

TEST_CASE("Baseline loads without asking for exclude patterns")
{
    const std::filesystem::path baseline_path = "test_baseline_no_out.json";

    fimlite::FileRecordMap records;

    records["kept.txt"] = fimlite::FileRecord
    {
        "kept.txt",
        "abc",
        3,
        0
    };

    fimlite::save_baseline(records, baseline_path, {"node_modules"});

    const auto loaded_records = fimlite::load_baseline(baseline_path);

    REQUIRE(loaded_records.size() == 1);
    REQUIRE(fimlite::load_baseline(baseline_path, nullptr).size() == 1);

    std::filesystem::remove(baseline_path);
}