#include "fimlite/cli.hpp"
#include "fimlite/baseline.hpp"
#include "fimlite/paths.hpp"
#include "fimlite/scanner.hpp"

#include <algorithm>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace fimlite
{

namespace
{

void print_usage(const std::string& program_name, std::ostream& out)
{
    out << "fimlite - File Integrity Monitoring Lite\n"
        << "Usage:\n"
        << "  " << program_name << " init <root_directory> <baseline_file> [--exclude <name> ...] [--hmac-key <key>]\n"
        << "  " << program_name << " check <root_directory> <baseline_file> [--exclude <name> ...] [--hmac-key <key>]\n"
        << "  " << program_name << " help, --help\n"
        << "Commands:\n"
        << "  init   Create a baseline of the specified directory.\n"
        << "  check  Check the specified directory against the baseline.\n"
        << "  help    Displays this help message and exits successfully.\n"
        << "Options:\n"
        << "  --exclude <name>   Skip files and directories with this name. Can be repeated.\n"
        << "  --hmac-key <key>   Sign the baseline on init and verify it on check with HMAC-SHA256.\n"
        << "                     Defaults to the FIMLITE_HMAC_KEY environment variable, which is preferred\n"
        << "                     because a key on the command line is visible to other users.\n"
        << "Exit codes:\n"
        << "  0  Success: no changes detected and every entry was read.\n"
        << "  1  Changes detected (check only). Takes precedence over 3.\n"
        << "  2  Invalid command line usage.\n"
        << "  3  Incomplete run: some entries could not be read. Symlinks that are not followed do not count.\n"
        << "  4  Runtime error, such as a missing or corrupt baseline or an I/O failure.\n"
        << "  5  Baseline signature verification failed: the baseline was modified, is not signed, or the key is wrong.\n";
}

void print_change(const Change& change)
{
    switch (change.type)
    {
        case ChangeType::Added:
            std::cout << "[ADDED] " << change.path << '\n';
            break;
        case ChangeType::Removed:
            std::cout << "[REMOVED] " << change.path << '\n';
            break;
        case ChangeType::Modified:
            std::cout << "[MODIFIED] " << change.path << '\n';
            break;
    }
}

void print_skipped(const std::vector<SkippedEntry>& skipped)
{
    std::size_t symlink_count = 0;
    std::vector<const SkippedEntry*> errors;

    for (const auto& entry : skipped)
    {
        if (entry.kind == SkipReason::Symlink)
        {
            ++symlink_count;
        }
        else
        {
            errors.push_back(&entry);
        }
    }

    if (!errors.empty())
    {
        std::cerr << "Warning: " << errors.size()
                  << (errors.size() == 1 ? " entry skipped due to errors:\n" : " entries skipped due to errors:\n");

        for (const auto* entry : errors)
        {
            std::cerr << "  " << entry->path << ": " << entry->reason << '\n';
        }
    }

    if (symlink_count > 0)
    {
        std::cerr << "Note: " << symlink_count
                  << (symlink_count == 1 ? " symlink not followed.\n" : " symlinks not followed.\n");
    }
}

bool has_scan_errors(const std::vector<SkippedEntry>& skipped)
{
    return std::any_of(skipped.begin(), skipped.end(),
                       [](const SkippedEntry& entry)
                       {
                           return entry.kind == SkipReason::Error;
                       });
}

std::string resolve_hmac_key(const std::string& cli_value)
{
    return cli_value.empty() ? utf8_env("FIMLITE_HMAC_KEY") : cli_value;
}

int run_init(const std::filesystem::path& root,
             const std::filesystem::path& baseline_path,
             const std::vector<std::string>& exclude_names,
             const std::string& hmac_key)
{
    std::vector<SkippedEntry> skipped;
    const auto records = scan_directory(root, exclude_names, &skipped);
    save_baseline(records, baseline_path, exclude_names, hmac_key);

    print_skipped(skipped);

    std::cout << "Baseline created: " << records.size() << " files scanned.\n";

    return has_scan_errors(skipped) ? 3 : 0;
}

int run_check(const std::filesystem::path& root,
              const std::filesystem::path& baseline_path,
              const std::vector<std::string>& exclude_names,
              const std::string& hmac_key)
{
    std::vector<std::string> stored_exclude_names;
    const auto baseline = load_baseline(baseline_path, &stored_exclude_names, hmac_key);

    const bool overridden = !exclude_names.empty();
    const std::vector<std::string>& effective_exclude_names =
        overridden ? exclude_names : stored_exclude_names;

    if (overridden && !exclude_names_equal_ignore_case(stored_exclude_names, exclude_names))
    {
        std::cerr << "Warning: --exclude differs from the patterns stored in the baseline; "
                     "using the command line values.\n";
    }

    std::vector<SkippedEntry> skipped;
    const auto current = scan_directory(root, effective_exclude_names, &skipped);
    const auto changes = diff(baseline, current);

    print_skipped(skipped);

    if (changes.empty())
    {
        std::cout << "No changes detected.\n";
        return has_scan_errors(skipped) ? 3 : 0;
    }

    for (const auto& change : changes)
    {
        print_change(change);
    }

    return 1;
}

} // namespace

int run_cli(const std::vector<std::string>& args)
{
    const std::string program_name = args.empty() ? "fim_lite" : args[0];

    if (args.size() < 2)
    {
        std::cerr << "Error: No command provided.\n";
        print_usage(program_name, std::cerr);
        return 2;
    }

    const std::string command = args[1];

    if (command == "help" || command == "--help")
    {
        if (args.size() > 2)
        {
            std::cerr << "Error: 'help' command does not take any additional arguments.\n";
            print_usage(program_name, std::cerr);
            return 2;
        }

        print_usage(program_name, std::cout);
        return 0;
    }

    if (command != "init" && command != "check")
    {
        std::cerr << "Error: Unknown command '" << command << "'.\n";
        print_usage(program_name, std::cerr);
        return 2;
    }

    if (args.size() < 4)
    {
        std::cerr << "Error: Command '" << command << "' missing arguments.\n"
                  << "Expected: "
                  << program_name
                  << " "
                  << command
                  << " <folder> <baseline.json> [--exclude <name> ...] [--hmac-key <key>]\n\n";

        print_usage(program_name, std::cerr);
        return 2;
    }

    std::vector<std::string> exclude_names;
    std::string hmac_key;

    for (std::size_t i = 4; i < args.size(); ++i)
    {
        const std::string argument = args[i];

        if (argument == "--exclude")
        {
            if (i + 1 >= args.size())
            {
                std::cerr << "Error: '--exclude' option requires a name argument.\n";
                return 2;
            }

            const std::string exclude_name = args[++i];

            if (exclude_name.empty())
            {
                std::cerr << "Error: '--exclude' option requires a non-empty name argument.\n";
                return 2;
            }

            exclude_names.push_back(exclude_name);
        }
        else if (argument == "--hmac-key")
        {
            if (i + 1 >= args.size())
            {
                std::cerr << "Error: '--hmac-key' option requires a key argument.\n";
                return 2;
            }

            if (!hmac_key.empty())
            {
                std::cerr << "Error: '--hmac-key' option can be given only once.\n";
                return 2;
            }

            hmac_key = args[++i];

            if (hmac_key.empty())
            {
                std::cerr << "Error: '--hmac-key' option requires a non-empty key argument.\n";
                return 2;
            }
        }
        else
        {
            std::cerr << "Error: Unknown option '" << argument << "'.\n";
            print_usage(program_name, std::cerr);
            return 2;
        }
    }

    try
    {
        const std::filesystem::path root = from_utf8(args[2]);
        const std::filesystem::path baseline_path = from_utf8(args[3]);
        const std::string effective_hmac_key = resolve_hmac_key(hmac_key);

        if (command == "init")
        {
            return run_init(root, baseline_path, exclude_names, effective_hmac_key);
        }
        else
        {
            return run_check(root, baseline_path, exclude_names, effective_hmac_key);
        }
    }
    catch (const BaselineSignatureError& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 5;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 4;
    }
}

} // namespace fimlite
