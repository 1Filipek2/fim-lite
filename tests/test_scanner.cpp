#include <catch2/catch_test_macros.hpp>

#include "fimlite/scanner.hpp"
#include "fimlite/hasher.hpp"
#include "fimlite/baseline.hpp"

#include <filesystem>
#include <fstream>

TEST_CASE("Diff detects added, removed and modified files")
{
    fimlite::FileRecordMap baseline;

    baseline["a.txt"] = fimlite::FileRecord
    {
        "a.txt",
        "111",
        10,
        1000
    };

    baseline["c.txt"] = fimlite::FileRecord
    {
        "c.txt",
        "333",
        30,
        3000
    };

    baseline["d.txt"] = fimlite::FileRecord
    {
        "d.txt",
        "444",
        40,
        4000
    };

    fimlite::FileRecordMap current;

    current["b.txt"] = fimlite::FileRecord
    {
        "b.txt",
        "222",
        20,
        2000
    };

    current["c.txt"] = fimlite::FileRecord
    {
        "c.txt",
        "999",
        30,
        3500
    };

    current["d.txt"] = fimlite::FileRecord
    {
        "d.txt",
        "444",
        40,
        4000
    };

    const auto changes = fimlite::diff(baseline, current);

    REQUIRE(changes.size() == 3);

    REQUIRE(changes[0].type == fimlite::ChangeType::Removed);
    REQUIRE(changes[0].path == "a.txt");

    REQUIRE(changes[1].type == fimlite::ChangeType::Added);
    REQUIRE(changes[1].path == "b.txt");

    REQUIRE(changes[2].type == fimlite::ChangeType::Modified);
    REQUIRE(changes[2].path == "c.txt");
}

TEST_CASE("Diff returns empty vector when maps are identical")
{
    fimlite::FileRecordMap baseline;

    baseline["a.txt"] = fimlite::FileRecord
    {
        "a.txt",
        "111",
        10,
        1000
    };

    fimlite::FileRecordMap current;

    current["a.txt"] = fimlite::FileRecord
    {
        "a.txt",
        "111",
        10,
        1000
    };

    const auto changes = fimlite::diff(baseline, current);

    REQUIRE(changes.empty());
}

TEST_CASE("Diff treats every entry as added when baseline is empty")
{
    fimlite::FileRecordMap baseline;

    fimlite::FileRecordMap current;

    current["a.txt"] = fimlite::FileRecord
    {
        "a.txt",
        "111",
        10,
        1000
    };

    current["b.txt"] = fimlite::FileRecord
    {
        "b.txt",
        "222",
        20,
        2000
    };

    const auto changes = fimlite::diff(baseline, current);

    REQUIRE(changes.size() == 2);

    REQUIRE(changes[0].type == fimlite::ChangeType::Added);
    REQUIRE(changes[0].path == "a.txt");

    REQUIRE(changes[1].type == fimlite::ChangeType::Added);
    REQUIRE(changes[1].path == "b.txt");
}

TEST_CASE("Diff treats every entry as removed when current is empty")
{
    fimlite::FileRecordMap baseline;

    baseline["a.txt"] = fimlite::FileRecord
    {
        "a.txt",
        "111",
        10,
        1000
    };

    baseline["b.txt"] = fimlite::FileRecord
    {
        "b.txt",
        "222",
        20,
        2000
    };

    fimlite::FileRecordMap current;

    const auto changes = fimlite::diff(baseline, current);

    REQUIRE(changes.size() == 2);

    REQUIRE(changes[0].type == fimlite::ChangeType::Removed);
    REQUIRE(changes[0].path == "a.txt");

    REQUIRE(changes[1].type == fimlite::ChangeType::Removed);
    REQUIRE(changes[1].path == "b.txt");
}

TEST_CASE("Scan directory finds regular files and computes matching hashes")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_scan";

    std::filesystem::create_directories(temp_dir);

    const std::filesystem::path file_path = temp_dir / "sample.txt";

    {
        std::ofstream out(file_path, std::ios::binary);
        out << "hello fim-lite";
    }

    const auto records = fimlite::scan_directory(temp_dir);

    REQUIRE(records.size() == 1);
    REQUIRE(records.find("sample.txt") != records.end());

    const auto& record = records.at("sample.txt");

    REQUIRE(record.path == "sample.txt");
    REQUIRE(record.size == std::filesystem::file_size(file_path));
    REQUIRE(record.hash == fimlite::sha256_file(file_path));

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Scan directory excludes specified file")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_exclude_file";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    {
        std::ofstream keep_file(temp_dir / "keep.txt");
        keep_file << "keep this file";

        std::ofstream exclude_file(temp_dir / "exclude.txt");
        exclude_file << "exclude this file";
    }

    const auto records = fimlite::scan_directory(temp_dir, {"exclude.txt"});

    REQUIRE(records.size() == 1);
    REQUIRE(records.find("keep.txt") != records.end());
    REQUIRE(records.find("exclude.txt") == records.end());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Scan directory excludes specified directory and its contents")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_exclude_dir";

    std::filesystem::remove_all(temp_dir);

    const auto src_dir = temp_dir / "src";
    const auto build_dir = temp_dir / "build";
    const auto nested_dir = build_dir / "nested";

    std::filesystem::create_directories(src_dir);
    std::filesystem::create_directories(nested_dir);

    {
        std::ofstream(src_dir / "main.cpp") << "int main() { return 0; }";
        std::ofstream(build_dir / "app.txt") << "build output";
        std::ofstream(nested_dir / "temp.txt") << "temporary file";
    }

    const auto records = fimlite::scan_directory(temp_dir, {"build"});

    REQUIRE(records.size() == 1);
    REQUIRE(records.find("src/main.cpp") != records.end());
    REQUIRE(records.find("build/app.txt") == records.end());
    REQUIRE(records.find("build/nested/temp.txt") == records.end());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Scan directory supports multiple exclude names")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_multiple_excludes";

    std::filesystem::remove_all(temp_dir);

    std::filesystem::create_directories(
        temp_dir / "build"
    );

    std::filesystem::create_directories(
        temp_dir / "logs"
    );

    {
        std::ofstream(temp_dir / "keep.txt") << "keep";
        std::ofstream(temp_dir / "ignore.txt") << "ignore";

        std::ofstream(temp_dir / "build" / "app.txt")
            << "build file";

        std::ofstream(temp_dir / "logs" / "log.txt")
            << "log file";
    }

    const auto records = fimlite::scan_directory(
        temp_dir,
        {
            "ignore.txt",
            "build",
            "logs"
        }
    );

    REQUIRE(records.size() == 1);

    REQUIRE(records.find("keep.txt") != records.end());

    REQUIRE(records.find("ignore.txt") == records.end());
    REQUIRE(records.find("build/app.txt") == records.end());
    REQUIRE(records.find("logs/log.txt") == records.end());

    std::filesystem::remove_all(temp_dir);
}