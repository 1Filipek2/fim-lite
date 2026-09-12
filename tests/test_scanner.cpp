#include <catch2/catch_test_macros.hpp>

#include "fimlite/scanner.hpp"
#include "fimlite/hasher.hpp"
#include "fimlite/baseline.hpp"
#include "fimlite/paths.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

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

TEST_CASE("Scan directory matches exclude names case-insensitively")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_exclude_case";

    std::filesystem::remove_all(temp_dir);

    const auto src_dir = temp_dir / "src";
    const auto build_dir = temp_dir / "Build";

    std::filesystem::create_directories(src_dir);
    std::filesystem::create_directories(build_dir);

    {
        std::ofstream(src_dir / "main.cpp") << "int main() { return 0; }";
        std::ofstream(build_dir / "app.txt") << "build output";
        std::ofstream(temp_dir / "Notes.TXT") << "notes";
    }

    const auto records = fimlite::scan_directory(
        temp_dir,
        {
            "build",
            "notes.txt"
        }
    );

    REQUIRE(records.size() == 1);
    REQUIRE(records.find("src/main.cpp") != records.end());
    REQUIRE(records.find("Build/app.txt") == records.end());
    REQUIRE(records.find("Notes.TXT") == records.end());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Scan directory matches exclude names case-sensitively outside ASCII")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_exclude_case_unicode";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto lower_name = fimlite::from_utf8(u8"zálohy.txt");

    {
        std::ofstream file(temp_dir / lower_name);

        if (!file)
        {
            std::filesystem::remove_all(temp_dir);
            SUCCEED("Skipped: non-ASCII file names are not supported here");
            return;
        }

        file << "content";
    }

    const auto records = fimlite::scan_directory(temp_dir, {u8"ZÁLOHY.TXT"});

    REQUIRE(records.size() == 1);
    REQUIRE(records.find(u8"zálohy.txt") != records.end());

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

TEST_CASE("Scan directory handles non-ASCII file names")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_unicode";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const std::vector<std::string> names =
    {
        u8"ascii.txt",
        u8"diakritika_č.txt",
        u8"azbuka_ф.txt",
        u8"cjk_测试.txt",
        u8"emoji_🙂.txt"
    };

    std::vector<std::string> created;

    for (const auto& name : names)
    {
        std::ofstream file(temp_dir / fimlite::from_utf8(name));

        if (!file)
        {
            continue;
        }

        file << "content";
        created.push_back(name);
    }

    REQUIRE(created.size() >= 2);

    std::vector<fimlite::SkippedEntry> skipped;

    const auto records = fimlite::scan_directory(temp_dir, {}, &skipped);

    REQUIRE(skipped.empty());
    REQUIRE(records.size() == created.size());

    for (const auto& name : created)
    {
        REQUIRE(records.count(name) == 1);
    }

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Scan directory does not follow file symlinks")
{
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_symlink";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir / "inside");
    std::filesystem::create_directories(temp_dir / "outside");

    {
        std::ofstream(temp_dir / "outside" / "target.txt") << "outside the scanned tree";
        std::ofstream(temp_dir / "inside" / "real.txt") << "inside";
    }

    std::error_code ec;
    std::filesystem::create_symlink(temp_dir / "outside" / "target.txt",
                                    temp_dir / "inside" / "link.txt",
                                    ec);

    if (ec)
    {
        std::filesystem::remove_all(temp_dir);
        SUCCEED("Skipped: creating symlinks is not permitted here");
        return;
    }

    std::vector<fimlite::SkippedEntry> skipped;

    const auto records = fimlite::scan_directory(temp_dir / "inside", {}, &skipped);

    REQUIRE(records.size() == 1);
    REQUIRE(records.find("real.txt") != records.end());
    REQUIRE(records.find("link.txt") == records.end());

    REQUIRE(skipped.size() == 1);
    REQUIRE(skipped[0].kind == fimlite::SkipReason::Symlink);
    REQUIRE(skipped[0].path == fimlite::to_utf8_native(temp_dir / "inside" / "link.txt"));

    std::filesystem::remove_all(temp_dir);
}

#ifndef _WIN32

TEST_CASE("Scan directory continues past an unreadable directory and reports it")
{
    if (::geteuid() == 0)
    {
        SUCCEED("Skipped: running as root, chmod 000 is not enforced");
        return;
    }

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_denied";

    std::filesystem::remove_all(temp_dir);

    const auto readable_dir = temp_dir / "a";
    const auto denied_dir = temp_dir / "denied";

    std::filesystem::create_directories(readable_dir);
    std::filesystem::create_directories(denied_dir);

    {
        std::ofstream(readable_dir / "f1.txt") << "one";
        std::ofstream(temp_dir / "top.txt") << "two";
    }

    std::filesystem::permissions(denied_dir, std::filesystem::perms::none);

    std::vector<fimlite::SkippedEntry> skipped;

    const auto records = fimlite::scan_directory(temp_dir, {}, &skipped);

    std::filesystem::permissions(denied_dir, std::filesystem::perms::owner_all);

    REQUIRE(records.size() == 2);

    REQUIRE(records.find("a/f1.txt") != records.end());
    REQUIRE(records.find("top.txt") != records.end());

    REQUIRE(skipped.size() == 1);
    REQUIRE(skipped[0].path == fimlite::to_utf8_native(denied_dir));

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Scan directory reports an unreadable file instead of failing the scan")
{
    if (::geteuid() == 0)
    {
        SUCCEED("Skipped: running as root, chmod 000 is not enforced");
        return;
    }

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fim_lite_test_unreadable_file";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto unreadable_file = temp_dir / "secret.txt";

    {
        std::ofstream(temp_dir / "keep.txt") << "keep";
        std::ofstream(unreadable_file) << "secret";
    }

    std::filesystem::permissions(unreadable_file, std::filesystem::perms::none);

    std::vector<fimlite::SkippedEntry> skipped;

    const auto records = fimlite::scan_directory(temp_dir, {}, &skipped);

    std::filesystem::permissions(unreadable_file, std::filesystem::perms::owner_all);

    REQUIRE(records.size() == 1);
    REQUIRE(records.find("keep.txt") != records.end());
    REQUIRE(records.find("secret.txt") == records.end());

    REQUIRE(skipped.size() == 1);
    REQUIRE(skipped[0].path == fimlite::to_utf8_native(unreadable_file));

    std::filesystem::remove_all(temp_dir);
}

#endif
