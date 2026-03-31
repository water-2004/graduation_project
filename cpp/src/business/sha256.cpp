#include "business/sha256.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace gp::backend {

namespace {

constexpr std::array<std::uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline std::uint32_t Rotr(std::uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

inline std::uint32_t Ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline std::uint32_t Maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t Sigma0(std::uint32_t x) {
    return Rotr(x, 2) ^ Rotr(x, 13) ^ Rotr(x, 22);
}

inline std::uint32_t Sigma1(std::uint32_t x) {
    return Rotr(x, 6) ^ Rotr(x, 11) ^ Rotr(x, 25);
}

inline std::uint32_t SmallSigma0(std::uint32_t x) {
    return Rotr(x, 7) ^ Rotr(x, 18) ^ (x >> 3);
}

inline std::uint32_t SmallSigma1(std::uint32_t x) {
    return Rotr(x, 17) ^ Rotr(x, 19) ^ (x >> 10);
}

struct Sha256State {
    std::array<std::uint32_t, 8> h = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    std::uint64_t total_bits = 0;
    std::array<std::uint8_t, 64> block{};
    std::size_t block_len = 0;

    void ProcessBlock() {
        std::array<std::uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                    (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                    (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                    (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = SmallSigma1(w[i - 2]) + w[i - 7] + SmallSigma0(w[i - 15]) + w[i - 16];
        }

        auto a = h[0], b = h[1], c = h[2], d = h[3];
        auto e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const auto t1 = hh + Sigma1(e) + Ch(e, f, g) + kK[i] + w[i];
            const auto t2 = Sigma0(a) + Maj(a, b, c);
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void Update(const void* data, std::size_t len) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        total_bits += len * 8;

        for (std::size_t i = 0; i < len; ++i) {
            block[block_len++] = bytes[i];
            if (block_len == 64) {
                ProcessBlock();
                block_len = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> Finalize() {
        block[block_len++] = 0x80;
        if (block_len > 56) {
            while (block_len < 64) {
                block[block_len++] = 0;
            }
            ProcessBlock();
            block_len = 0;
        }
        while (block_len < 56) {
            block[block_len++] = 0;
        }

        for (int i = 7; i >= 0; --i) {
            block[block_len++] = static_cast<std::uint8_t>(total_bits >> (i * 8));
        }
        ProcessBlock();

        std::array<std::uint8_t, 32> digest{};
        for (int i = 0; i < 8; ++i) {
            digest[i * 4] = static_cast<std::uint8_t>(h[i] >> 24);
            digest[i * 4 + 1] = static_cast<std::uint8_t>(h[i] >> 16);
            digest[i * 4 + 2] = static_cast<std::uint8_t>(h[i] >> 8);
            digest[i * 4 + 3] = static_cast<std::uint8_t>(h[i]);
        }
        return digest;
    }
};

}  // namespace

std::string Sha256Hex(const std::string& input) {
    Sha256State state;
    state.Update(input.data(), input.size());
    const auto digest = state.Finalize();

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (auto byte : digest) {
        stream << std::setw(2) << static_cast<int>(byte);
    }
    return stream.str();
}

bool IsSha256Hex(const std::string& text) {
    if (text.size() != 64) {
        return false;
    }
    for (char c : text) {
        const bool is_hex = (c >= '0' && c <= '9') ||
                            (c >= 'a' && c <= 'f') ||
                            (c >= 'A' && c <= 'F');
        if (!is_hex) {
            return false;
        }
    }
    return true;
}

}  // namespace gp::backend
