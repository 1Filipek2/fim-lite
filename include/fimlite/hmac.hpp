#pragma once

#include <string>

namespace fimlite
{

std::string hmac_sha256_hex(const std::string& key, const std::string& data);
bool hmac_hex_equal(const std::string& left, const std::string& right);

} // namespace fimlite
