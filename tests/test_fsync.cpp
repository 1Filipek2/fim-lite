#include <catch2/catch_test_macros.hpp>

#include "fimlite/fsync.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

TEST_CASE("Sync file succeeds on an existing file")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_sync_existing";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto file_path = temp_dir / "synced.txt";
    std::ofstream(file_path) << "synced";

    REQUIRE_NOTHROW(fimlite::sync_file(file_path));
    REQUIRE_NOTHROW(fimlite::sync_parent_directory(file_path));

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Sync file reports a missing file")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_sync_missing";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    REQUIRE_THROWS_AS(fimlite::sync_file(temp_dir / "missing.txt"), std::system_error);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Sync parent directory accepts a bare file name")
{
    REQUIRE_NOTHROW(fimlite::sync_parent_directory("baseline.json"));
}

#ifndef _WIN32

TEST_CASE("Sync parent directory reports a missing directory")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_sync_missing_directory";

    std::filesystem::remove_all(temp_dir);

    REQUIRE_THROWS_AS(fimlite::sync_parent_directory(temp_dir / "baseline.json"), std::system_error);
}

#endif
