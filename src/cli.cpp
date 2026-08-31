#include "fimlite/cli.hpp"
#include "fimlite/baseline.hpp"
#include "fimlite/scanner.hpp"

#include <iostream>
#include <filesystem>

namespace fimlite
{

namespace 
{

void print_usage(const char* program_name)
{
    std::cerr << "Usage:\n"
        << "  " << program_name << " init <folder> baseline.json\n"
        << "  " << program_name << " check <folder> baseline.json\n";
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

int run_init(const std::filesystem::path& root, const std::filesystem::path& baseline_path)
{
    const auto records = scan_directory(root);
    save_baseline(records, baseline_path);

    std::cout << "Baseline created: " << records.size() << " files scanned.\n";

    return 0;
}

int run_check(const std::filesystem::path& root, const std::filesystem::path& baseline_path)
{
    const auto baseline = load_baseline(baseline_path);
    const auto current = scan_directory(root);
    const auto changes = diff(baseline, current);

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
    if (argc != 4)
    {
        print_usage(argv[0]);
        return 1;
    }

    const std::string command = argv[1];
    const std::filesystem::path root = argv[2];
    const std::filesystem::path baseline_path = argv[3];

    try
    {
        if (command == "init")
        {
            return run_init(root, baseline_path);
        }
        else if (command == "check")
        {
            return run_check(root, baseline_path);
        }
        else
        {
            print_usage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

}

} // namespace fimlite