#include <catch2/catch_test_macros.hpp>

#include "fimlite/baseline.hpp"
#include "fimlite/hmac.hpp"
#include "fimlite/scanner.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace
{

std::filesystem::path make_temp_dir(const std::string& name)
{
    const auto dir = std::filesystem::temp_directory_path() / name;

    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    return dir;
}

fimlite::FileRecordMap make_records()
{
    fimlite::FileRecordMap records;

    records["tracked.txt"] = fimlite::FileRecord
    {
        "tracked.txt",
        "abc123",
        6,
        123456789
    };

    return records;
}

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void write_text(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream(path, std::ios::binary) << text;
}

void replace_once(std::string& text, const std::string& from, const std::string& to)
{
    const auto position = text.find(from);

    REQUIRE(position != std::string::npos);

    text.replace(position, from.size(), to);
}

std::string load_capturing_stderr(const std::filesystem::path& path,
                                  const std::string& hmac_key,
                                  fimlite::FileRecordMap& records)
{
    std::ostringstream sink;
    auto* const previous_err = std::cerr.rdbuf(sink.rdbuf());

    try
    {
        records = fimlite::load_baseline(path, nullptr, hmac_key);
    }
    catch (...)
    {
        std::cerr.rdbuf(previous_err);
        throw;
    }

    std::cerr.rdbuf(previous_err);

    return sink.str();
}

} // namespace

TEST_CASE("HMAC-SHA256 matches RFC 4231 test vectors")
{
    REQUIRE(fimlite::hmac_sha256_hex(std::string(20, '\x0b'), "Hi There") ==
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    REQUIRE(fimlite::hmac_sha256_hex("Jefe", "what do ya want for nothing?") ==
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST_CASE("HMAC hex comparison requires identical strings")
{
    REQUIRE(fimlite::hmac_hex_equal("abcdef", "abcdef"));
    REQUIRE_FALSE(fimlite::hmac_hex_equal("abcdef", "abcdee"));
    REQUIRE_FALSE(fimlite::hmac_hex_equal("abcdef", "abcde"));
    REQUIRE_FALSE(fimlite::hmac_hex_equal("", "abcdef"));
}

TEST_CASE("Signed baseline loads with the same key")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_same_key");
    const auto baseline_path = temp_dir / "baseline.json";
    const auto records = make_records();
    const std::vector<std::string> exclude_names{"ignored"};

    fimlite::save_baseline(records, baseline_path, exclude_names, "secret");

    std::vector<std::string> loaded_exclude_names;
    const auto loaded = fimlite::load_baseline(baseline_path, &loaded_exclude_names, "secret");

    REQUIRE(fimlite::diff(records, loaded).empty());
    REQUIRE(loaded_exclude_names == exclude_names);
    REQUIRE(read_text(baseline_path).find("\"hmac\"") != std::string::npos);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Tampered file records fail signature verification")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_tampered_files");
    const auto baseline_path = temp_dir / "baseline.json";

    fimlite::save_baseline(make_records(), baseline_path, {}, "secret");

    std::string text = read_text(baseline_path);
    replace_once(text, "abc123", "abc124");
    write_text(baseline_path, text);

    REQUIRE_THROWS_AS(fimlite::load_baseline(baseline_path, nullptr, "secret"), fimlite::BaselineSignatureError);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Tampered exclude patterns fail signature verification")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_tampered_exclude");
    const auto baseline_path = temp_dir / "baseline.json";

    fimlite::save_baseline(make_records(), baseline_path, {"ignored"}, "secret");

    std::string text = read_text(baseline_path);
    replace_once(text, "\"ignored\"", "\"tracked.txt\"");
    write_text(baseline_path, text);

    REQUIRE_THROWS_AS(fimlite::load_baseline(baseline_path, nullptr, "secret"), fimlite::BaselineSignatureError);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Signed baseline fails verification with a wrong key")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_wrong_key");
    const auto baseline_path = temp_dir / "baseline.json";

    fimlite::save_baseline(make_records(), baseline_path, {}, "secret");

    REQUIRE_THROWS_AS(fimlite::load_baseline(baseline_path, nullptr, "wrong-key"), fimlite::BaselineSignatureError);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Signed baseline loads without a key but warns that it was not verified")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_no_key");
    const auto baseline_path = temp_dir / "baseline.json";
    const auto records = make_records();

    fimlite::save_baseline(records, baseline_path, {}, "secret");

    fimlite::FileRecordMap loaded;
    const std::string warning = load_capturing_stderr(baseline_path, "", loaded);

    REQUIRE(fimlite::diff(records, loaded).empty());
    REQUIRE(warning.find("NOT verified") != std::string::npos);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Unsigned baseline fails verification when a key is given")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_unsigned_with_key");
    const auto baseline_path = temp_dir / "baseline.json";

    fimlite::save_baseline(make_records(), baseline_path);

    REQUIRE(read_text(baseline_path).find("\"hmac\"") == std::string::npos);
    REQUIRE_THROWS_AS(fimlite::load_baseline(baseline_path, nullptr, "secret"), fimlite::BaselineSignatureError);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Stripping the signature fails verification when a key is given")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_stripped");
    const auto baseline_path = temp_dir / "baseline.json";

    fimlite::save_baseline(make_records(), baseline_path, {}, "secret");

    auto signed_baseline = nlohmann::json::parse(read_text(baseline_path));
    signed_baseline.erase("hmac");
    write_text(baseline_path, signed_baseline.dump(4));

    REQUIRE_THROWS_AS(fimlite::load_baseline(baseline_path, nullptr, "secret"), fimlite::BaselineSignatureError);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Unsigned baseline loads without a key and without a warning")
{
    const auto temp_dir = make_temp_dir("fim_lite_test_hmac_unsigned_no_key");
    const auto baseline_path = temp_dir / "baseline.json";
    const auto records = make_records();

    fimlite::save_baseline(records, baseline_path);

    fimlite::FileRecordMap loaded;
    const std::string warning = load_capturing_stderr(baseline_path, "", loaded);

    REQUIRE(fimlite::diff(records, loaded).empty());
    REQUIRE(warning.empty());

    std::filesystem::remove_all(temp_dir);
}
