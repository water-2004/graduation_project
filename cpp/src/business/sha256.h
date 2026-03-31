#pragma once

#include <string>

namespace gp::backend {

// Returns lowercase hex-encoded SHA-256 hash of the input string.
std::string Sha256Hex(const std::string& input);

// Returns true if the string looks like a SHA-256 hex digest (exactly 64 hex chars).
bool IsSha256Hex(const std::string& text);

}  // namespace gp::backend
