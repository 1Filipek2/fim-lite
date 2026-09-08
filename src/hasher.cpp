#include "fimlite/hasher.hpp"
#include "fimlite/paths.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

#include <openssl/evp.h>

namespace fimlite
{

struct Sha256Hasher::Impl
{
    EVP_MD_CTX* ctx;

    Impl()
        : ctx(EVP_MD_CTX_new())
    {
        if (!ctx) 
        {
            throw std::runtime_error("EVP_MD_CTX_new failed");
        }

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
        {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("DigestInit failed");
        }
    }

    ~Impl()
    {
        EVP_MD_CTX_free(ctx);
    }
};

Sha256Hasher::Sha256Hasher() : impl_(std::make_unique<Impl>())
{
}

Sha256Hasher::~Sha256Hasher() = default;

void Sha256Hasher::update(const void* data, std::size_t len)
{
    if (EVP_DigestUpdate(impl_->ctx, data, len) != 1)
    {
        throw std::runtime_error("EVP_DigestUpdate failed");
    }
}

std::string Sha256Hasher::finalize() 
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestFinal_ex(impl_->ctx, digest, &digest_len) != 1)
    {
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    std::ostringstream hex;

    for (unsigned int i = 0; i < digest_len; ++i)
    {
        hex << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return hex.str();
}

std::string sha256_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("Failed to open file: " + to_utf8_native(path));
    }

    Sha256Hasher hasher;

    constexpr std::size_t buffer_size = 64 * 1024;
    std::array<char, buffer_size> buffer{};

    while (file)
    {
        file.read(buffer.data(), buffer_size);
        const std::streamsize bytes_read = file.gcount();

        if (bytes_read > 0)
        {
            hasher.update(buffer.data(), static_cast<std::size_t>(bytes_read));
        }
    }

    if (!file.eof())
    {
        throw std::runtime_error("Failed to read file: " + to_utf8_native(path));
    }

    return hasher.finalize();
}

} // namespace fimlite