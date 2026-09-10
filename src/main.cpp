#include "fimlite/cli.hpp"

#include <string>
#include <vector>

#ifdef _WIN32

#include "fimlite/paths.hpp"

#include <exception>
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
    std::vector<std::string> args;

    try
    {
        args = fimlite::utf8_args(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: Invalid command line argument: " << e.what() << '\n';
        return 2;
    }

    return fimlite::run_cli(args);
}

#else

int main(int argc, char** argv)
{
    return fimlite::run_cli(std::vector<std::string>(argv, argv + argc));
}

#endif
