#include <catch2/catch_test_macros.hpp>

#include "fimlite/cli.hpp"
#include "fimlite/paths.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace
{

int run_quiet(const std::vector<std::string>& args)
{
    std::ostringstream sink;
    auto* const previous_out = std::cout.rdbuf(sink.rdbuf());
    auto* const previous_err = std::cerr.rdbuf(sink.rdbuf());

    const int code = fimlite::run_cli(args);

    std::cout.rdbuf(previous_out);
    std::cerr.rdbuf(previous_err);

    return code;
}

std::filesystem::path make_temp_dir(const std::string& name)
{
    const auto dir = std::filesystem::temp_directory_path() / name;

    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    return dir;
}

} // namespace

TEST_CASE("CLI returns 2 for invalid usage")
{
    REQUIRE(run_quiet({"fim_lite"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "verify"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "check", "root_only"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--exclude"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--unknown"}) == 2);
}

TEST_CASE("CLI returns 4 for runtime errors")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_runtime_error");

    const auto root = fimlite::to_utf8_native(temp_dir);
    const auto missing_root = fimlite::to_utf8_native(temp_dir / "missing_dir");
    const auto missing_baseline = fimlite::to_utf8_native(temp_dir / "missing.json");
    const auto corrupt_baseline = temp_dir / "corrupt.json";

    std::ofstream(corrupt_baseline) << "not json";

    REQUIRE(run_quiet({"fim_lite", "check", root, missing_baseline}) == 4);
    REQUIRE(run_quiet({"fim_lite", "check", root, fimlite::to_utf8_native(corrupt_baseline)}) == 4);
    REQUIRE(run_quiet({"fim_lite", "init", missing_root, fimlite::to_utf8_native(temp_dir / "b.json")}) == 4);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Check returns 0 without changes and 1 with changes")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_check");
    const auto data_dir = temp_dir / "data";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "file.txt") << "original";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    REQUIRE(run_quiet({"fim_lite", "init", root, baseline}) == 0);
    REQUIRE(run_quiet({"fim_lite", "check", root, baseline}) == 0);

    std::ofstream(data_dir / "file.txt") << "modified";

    REQUIRE(run_quiet({"fim_lite", "check", root, baseline}) == 1);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Symlinks do not make init or check incomplete")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_symlink");
    const auto data_dir = temp_dir / "data";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "real.txt") << "real";
    std::ofstream(temp_dir / "target.txt") << "target";

    std::error_code ec;
    std::filesystem::create_symlink(temp_dir / "target.txt", data_dir / "link.txt", ec);

    if (ec)
    {
        std::filesystem::remove_all(temp_dir);
        SUCCEED("Skipped: creating symlinks is not permitted here");
        return;
    }

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    REQUIRE(run_quiet({"fim_lite", "init", root, baseline}) == 0);
    REQUIRE(run_quiet({"fim_lite", "check", root, baseline}) == 0);

    std::filesystem::remove_all(temp_dir);
}

#ifndef _WIN32

TEST_CASE("Init and check return 3 when a directory cannot be read")
{
    if (::geteuid() == 0)
    {
        SUCCEED("Skipped: running as root, chmod 000 is not enforced");
        return;
    }

    const auto temp_dir = make_temp_dir("fim_lite_test_cli_incomplete");
    const auto data_dir = temp_dir / "data";
    const auto denied_dir = data_dir / "denied";

    std::filesystem::create_directories(denied_dir);
    std::ofstream(data_dir / "ok.txt") << "ok";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");
    const auto incomplete_baseline = fimlite::to_utf8_native(temp_dir / "incomplete.json");

    const int complete_init_code = run_quiet({"fim_lite", "init", root, baseline});

    std::filesystem::permissions(denied_dir, std::filesystem::perms::none);

    const int check_code = run_quiet({"fim_lite", "check", root, baseline});
    const int incomplete_init_code = run_quiet({"fim_lite", "init", root, incomplete_baseline});

    std::filesystem::permissions(denied_dir, std::filesystem::perms::owner_all);

    REQUIRE(complete_init_code == 0);
    REQUIRE(check_code == 3);
    REQUIRE(incomplete_init_code == 3);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Check returns 1 rather than 3 when there are changes and unreadable entries")
{
    if (::geteuid() == 0)
    {
        SUCCEED("Skipped: running as root, chmod 000 is not enforced");
        return;
    }

    const auto temp_dir = make_temp_dir("fim_lite_test_cli_changes_and_incomplete");
    const auto data_dir = temp_dir / "data";
    const auto denied_dir = data_dir / "denied";

    std::filesystem::create_directories(denied_dir);
    std::ofstream(data_dir / "ok.txt") << "ok";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    const int init_code = run_quiet({"fim_lite", "init", root, baseline});

    std::ofstream(data_dir / "ok.txt") << "changed";
    std::filesystem::permissions(denied_dir, std::filesystem::perms::none);

    const int check_code = run_quiet({"fim_lite", "check", root, baseline});

    std::filesystem::permissions(denied_dir, std::filesystem::perms::owner_all);

    REQUIRE(init_code == 0);
    REQUIRE(check_code == 1);

    std::filesystem::remove_all(temp_dir);
}

#endif
