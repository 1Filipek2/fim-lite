#include <catch2/catch_test_macros.hpp>

#include "fimlite/baseline.hpp"
#include "fimlite/paths.hpp"
#include "fimlite/scanner.hpp"

#include <filesystem>

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