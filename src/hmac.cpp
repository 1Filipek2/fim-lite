#include "fimlite/hmac.hpp"

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fimlite
{

std::string hmac_sha256_hex(const std::string& key, const std::string& data)
{
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);

    if (!mac)
    {
        throw std::runtime_error("EVP_MAC_fetch failed");
    }

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);

    if (!ctx)
    {
        throw std::runtime_error("EVP_MAC_CTX_new failed");
    }

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                         const_cast<char*>("SHA256"), 0),
        OSSL_PARAM_construct_end()
    };

    unsigned char digest[EVP_MAX_MD_SIZE];
    std::size_t digest_len = 0;

    const bool ok =
        EVP_MAC_init(ctx, reinterpret_cast<const unsigned char*>(key.data()), key.size(), params) == 1 &&
        EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(data.data()), data.size()) == 1 &&
        EVP_MAC_final(ctx, digest, &digest_len, sizeof(digest)) == 1;

    EVP_MAC_CTX_free(ctx);

    if (!ok)
    {
        throw std::runtime_error("HMAC computation failed");
    }

    std::ostringstream hex;

    for (std::size_t i = 0; i < digest_len; ++i)
    {
        hex << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }

    return hex.str();
}

bool hmac_hex_equal(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

} // namespace fimlite
