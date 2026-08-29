#include "../include/fimlite/hasher.hpp"

#include <fstream>
#include <iostream>
#include <string>

int main()
{
    const std::string filename = "test.txt";

    {
        std::ofstream file(filename);
        file << "test test hello test"; // expected hash 021d2768b560f0730cc7df61a68b5fcdbeb60832f5f467e1bfa7c93cc94b9ea3
    }

    try
    {
        const std::string hash = fimlite::sha256_file(filename);

        std::cout << "SHA-256: " << hash << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}