#pragma once 

#include <cstddef>
#include <string>
#include <filesystem>
#include <memory>

namespace fimlite
{

class Sha256Hasher
{
public:
    Sha256Hasher();
    ~Sha256Hasher();

    Sha256Hasher(const Sha256Hasher&) = delete;
    Sha256Hasher& operator = (const Sha256Hasher&) = delete;

    void update(const void* data, std::size_t len);
    std::string finalize();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string sha256_file(const std::filesystem::path&);

} // namespace fimlite