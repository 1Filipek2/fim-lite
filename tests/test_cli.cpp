#include <catch2/catch_test_macros.hpp>

#include "fimlite/cli.hpp"
#include "fimlite/paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <cstdio>
#include <share.h>
#else
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

void set_hmac_key_env(const std::string& value)
{
#ifdef _WIN32
    _wputenv_s(L"FIMLITE_HMAC_KEY", fimlite::from_utf8(value).c_str());
#else
    ::setenv("FIMLITE_HMAC_KEY", value.c_str(), 1);
#endif
}

void clear_hmac_key_env()
{
#ifdef _WIN32
    _wputenv_s(L"FIMLITE_HMAC_KEY", L"");
#else
    ::unsetenv("FIMLITE_HMAC_KEY");
#endif
}

} // namespace

TEST_CASE("CLI returns 2 for invalid usage")
{
    REQUIRE(run_quiet({"fim_lite"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "verify"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "check", "root_only"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--exclude"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--unknown"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--hmac-key"}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--hmac-key", ""}) == 2);
    REQUIRE(run_quiet({"fim_lite", "init", "root", "baseline.json", "--hmac-key", "a", "--hmac-key", "b"}) == 2);
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

TEST_CASE("Check reuses the exclude patterns stored in the baseline")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_exclude_persistence");
    const auto data_dir = temp_dir / "data";
    const auto ignored_dir = data_dir / "ignored";

    std::filesystem::create_directories(ignored_dir);
    std::ofstream(data_dir / "tracked.txt") << "tracked";
    std::ofstream(ignored_dir / "noise.txt") << "noise";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    REQUIRE(run_quiet({"fim_lite", "init", root, baseline, "--exclude", "ignored"}) == 0);

    std::ofstream(ignored_dir / "more_noise.txt") << "more noise";

    REQUIRE(run_quiet({"fim_lite", "check", root, baseline}) == 0);

    std::ofstream(data_dir / "tracked.txt") << "changed";

    REQUIRE(run_quiet({"fim_lite", "check", root, baseline}) == 1);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Command line excludes override the stored patterns")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_exclude_override");
    const auto data_dir = temp_dir / "data";
    const auto ignored_dir = data_dir / "ignored";

    std::filesystem::create_directories(ignored_dir);
    std::ofstream(data_dir / "tracked.txt") << "tracked";
    std::ofstream(ignored_dir / "noise.txt") << "noise";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    REQUIRE(run_quiet({"fim_lite", "init", root, baseline, "--exclude", "ignored"}) == 0);

    REQUIRE(run_quiet({"fim_lite", "check", root, baseline, "--exclude", "tracked.txt"}) == 1);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Check matches stored excludes regardless of letter case")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_exclude_case");
    const auto data_dir = temp_dir / "data";
    const auto ignored_dir = data_dir / "Ignored";

    std::filesystem::create_directories(ignored_dir);
    std::ofstream(data_dir / "tracked.txt") << "tracked";
    std::ofstream(ignored_dir / "noise.txt") << "noise";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    REQUIRE(run_quiet({"fim_lite", "init", root, baseline, "--exclude", "ignored"}) == 0);

    REQUIRE(run_quiet({"fim_lite", "check", root, baseline, "--exclude", "IGNORED"}) == 0);

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

TEST_CASE("Check verifies the baseline with the key given by --hmac-key")
{
    clear_hmac_key_env();

    const auto temp_dir = make_temp_dir("fim_lite_test_cli_hmac_flag");
    const auto data_dir = temp_dir / "data";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "file.txt") << "original";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    REQUIRE(run_quiet({"fim_lite", "init", root, baseline, "--hmac-key", "secret"}) == 0);
    REQUIRE(run_quiet({"fim_lite", "check", root, baseline, "--hmac-key", "secret"}) == 0);
    REQUIRE(run_quiet({"fim_lite", "check", root, baseline, "--hmac-key", "wrong-key"}) == 5);
    REQUIRE(run_quiet({"fim_lite", "check", root, baseline}) == 0);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Check returns 5 for an unsigned or tampered baseline when a key is given")
{
    clear_hmac_key_env();

    const auto temp_dir = make_temp_dir("fim_lite_test_cli_hmac_exit_code");
    const auto data_dir = temp_dir / "data";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "file.txt") << "original";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto unsigned_baseline = temp_dir / "unsigned.json";
    const auto signed_baseline = temp_dir / "signed.json";

    REQUIRE(run_quiet({"fim_lite", "init", root, fimlite::to_utf8_native(unsigned_baseline)}) == 0);
    REQUIRE(run_quiet({"fim_lite", "check", root, fimlite::to_utf8_native(unsigned_baseline), "--hmac-key", "secret"}) == 5);

    REQUIRE(run_quiet({"fim_lite", "init", root, fimlite::to_utf8_native(signed_baseline), "--hmac-key", "secret"}) == 0);

    std::ifstream original(signed_baseline, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
    original.close();

    const std::string stored_exclude = "\"exclude\": []";
    const auto exclude_position = text.find(stored_exclude);

    REQUIRE(exclude_position != std::string::npos);

    text.replace(exclude_position, stored_exclude.size(), "\"exclude\": [\"file.txt\"]");
    std::ofstream(signed_baseline, std::ios::binary) << text;

    REQUIRE(run_quiet({"fim_lite", "check", root, fimlite::to_utf8_native(signed_baseline), "--hmac-key", "secret"}) == 5);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("HMAC key comes from FIMLITE_HMAC_KEY and --hmac-key overrides it")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_cli_hmac_env");
    const auto data_dir = temp_dir / "data";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "file.txt") << "original";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");
    const std::string env_key = u8"kľúč-z-prostredia";

    set_hmac_key_env(env_key);

    const int init_code = run_quiet({"fim_lite", "init", root, baseline});
    const int check_code = run_quiet({"fim_lite", "check", root, baseline});
    const int override_code = run_quiet({"fim_lite", "check", root, baseline, "--hmac-key", "other-key"});

    clear_hmac_key_env();

    const int flag_code = run_quiet({"fim_lite", "check", root, baseline, "--hmac-key", env_key});

    REQUIRE(init_code == 0);
    REQUIRE(check_code == 0);
    REQUIRE(override_code == 5);
    REQUIRE(flag_code == 0);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Check returns 3 rather than reporting a file that cannot be read as removed")
{
#ifndef _WIN32
    if (::geteuid() == 0)
    {
        SUCCEED("Skipped: running as root, chmod 000 is not enforced");
        return;
    }
#endif

    clear_hmac_key_env();

    const auto temp_dir = make_temp_dir("fim_lite_test_cli_unreadable_file");
    const auto data_dir = temp_dir / "data";
    const auto blocked_file = data_dir / "blocked.txt";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "ok.txt") << "ok";
    std::ofstream(blocked_file) << "blocked";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");

    const int init_code = run_quiet({"fim_lite", "init", root, baseline});

#ifdef _WIN32
    FILE* const lock = _wfsopen(blocked_file.c_str(), L"rb", _SH_DENYRW);
    REQUIRE(lock != nullptr);
#else
    std::filesystem::permissions(blocked_file, std::filesystem::perms::none);
#endif

    const int check_code = run_quiet({"fim_lite", "check", root, baseline});

#ifdef _WIN32
    std::fclose(lock);
#else
    std::filesystem::permissions(blocked_file, std::filesystem::perms::owner_all);
#endif

    REQUIRE(init_code == 0);
    REQUIRE(check_code == 3);

    std::filesystem::remove_all(temp_dir);
}

#ifndef _WIN32

TEST_CASE("Init and check return 4 when the root directory cannot be read")
{
    if (::geteuid() == 0)
    {
        SUCCEED("Skipped: running as root, chmod 000 is not enforced");
        return;
    }

    clear_hmac_key_env();

    const auto temp_dir = make_temp_dir("fim_lite_test_cli_unreadable_root");
    const auto data_dir = temp_dir / "data";

    std::filesystem::create_directories(data_dir);
    std::ofstream(data_dir / "file.txt") << "file";

    const auto root = fimlite::to_utf8_native(data_dir);
    const auto baseline = fimlite::to_utf8_native(temp_dir / "baseline.json");
    const auto empty_baseline = fimlite::to_utf8_native(temp_dir / "empty.json");

    const int readable_init_code = run_quiet({"fim_lite", "init", root, baseline});

    std::filesystem::permissions(data_dir, std::filesystem::perms::none);

    const int init_code = run_quiet({"fim_lite", "init", root, empty_baseline});
    const int check_code = run_quiet({"fim_lite", "check", root, baseline});

    std::filesystem::permissions(data_dir, std::filesystem::perms::owner_all);

    REQUIRE(readable_init_code == 0);
    REQUIRE(init_code == 4);
    REQUIRE(check_code == 4);
    REQUIRE_FALSE(std::filesystem::exists(temp_dir / "empty.json"));

    std::filesystem::remove_all(temp_dir);
}

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
    std::ofstream(denied_dir / "hidden.txt") << "hidden";

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
