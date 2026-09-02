#include "fimlite/cli.hpp"
#include "fimlite/baseline.hpp"
#include "fimlite/scanner.hpp"

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace fimlite
{

namespace 
{

void print_usage(const char* program_name, std::ostream& out)
{
    out << "fimlite - File Integrity Monitoring Lite\n"
        << "Usage:\n"
        << "  " << program_name << " init <root_directory> <baseline_file> [--exclude <name> ...]\n"
        << "  " << program_name << " check <root_directory> <baseline_file> [--exclude <name> ...]\n"
        << "  " << program_name << " help, --help\n"
        << "Commands:\n"
        << "  init   Create a baseline of the specified directory.\n"
        << "  check  Check the specified directory against the baseline.\n"
        << "  help    Displays this help message and exits successfully.\n";
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
    if (skipped.empty())
    {
        return;
    }

    std::cerr << "Warning: " << skipped.size()
              << (skipped.size() == 1 ? " entry skipped due to errors:\n" : " entries skipped due to errors:\n");

    for (const auto& entry : skipped)
    {
        std::cerr << "  " << entry.path << ": " << entry.reason << '\n';
    }
}

int run_init(const std::filesystem::path& root,
             const std::filesystem::path& baseline_path,
             const std::vector<std::string>& exclude_names)
{
    std::vector<SkippedEntry> skipped;
    const auto records = scan_directory(root, exclude_names, &skipped);
    save_baseline(records, baseline_path);

    print_skipped(skipped);

    std::cout << "Baseline created: " << records.size() << " files scanned.\n";

    return skipped.empty() ? 0 : 3;
}

int run_check(const std::filesystem::path& root,
              const std::filesystem::path& baseline_path,
              const std::vector<std::string>& exclude_names)
{
    const auto baseline = load_baseline(baseline_path);

    std::vector<SkippedEntry> skipped;
    const auto current = scan_directory(root, exclude_names, &skipped);
    const auto changes = diff(baseline, current);

    print_skipped(skipped);

    if (changes.empty())
    {
        std::cout << "No changes detected.\n";
        return 0;
    }

    for (const auto& change : changes)
    {
        print_change(change);
    }

    return 1;
}

} // namespace

int run_cli(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Error: No command provided.\n";
        print_usage(argv[0], std::cerr);
        return 2;
    }

    const std::string command = argv[1];

    if (command == "help" || command == "--help")
    {
        if (argc > 2)
        {
            std::cerr << "Error: 'help' command does not take any additional arguments.\n";
            print_usage(argv[0], std::cerr);
            return 2;
        }

        print_usage(argv[0], std::cout);
        return 0;
    }

    if (command != "init" && command != "check")
    {
        std::cerr << "Error: Unknown command '" << command << "'.\n";
        print_usage(argv[0], std::cerr);
        return 2;
    }

    if (argc < 4)
    {
        std::cerr << "Error: Command '" << command << "' missing arguments.\n"
                  << "Expected: " 
                  << argv[0] 
                  << " " 
                  << command 
                  << " <folder> <baseline.json> [--exclude <name> ...]\n\n";
                  
        print_usage(argv[0], std::cerr);
        return 2;
    }

    const std::filesystem::path root = argv[2];
    const std::filesystem::path baseline_path = argv[3];

    std::vector<std::string> exclude_names;

    for (int i = 4; i < argc; ++i)
    {
        const std::string argument = argv[i];

        if (argument == "--exclude")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: '--exclude' option requires a name argument.\n";
                return 2;
            }

            const std::string exclude_name = argv[++i];

            if (exclude_name.empty())
            {
                std::cerr << "Error: '--exclude' option requires a non-empty name argument.\n";
                return 2;
            }

            exclude_names.push_back(exclude_name);
        }
        else
        {
            std::cerr << "Error: Unknown option '" << argument << "'.\n";
            print_usage(argv[0], std::cerr);
            return 2;
        }
    }

    try
    {
        if (command == "init")
        {
            return run_init(root, baseline_path, exclude_names);
        }
        else
        {
            return run_check(root, baseline_path, exclude_names);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

} // namespace fimlite