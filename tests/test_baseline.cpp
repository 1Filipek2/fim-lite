#include <catch2/catch_test_macros.hpp>

#include "fimlite/baseline.hpp"

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