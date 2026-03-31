#include "business/sha256.h"

#include <gtest/gtest.h>

using gp::backend::IsSha256Hex;
using gp::backend::Sha256Hex;

TEST(Sha256Test, EmptyString) {
    EXPECT_EQ(Sha256Hex(""),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, KnownVector123456) {
    EXPECT_EQ(Sha256Hex("123456"),
              "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92");
}

TEST(Sha256Test, KnownVectorHelloWorld) {
    EXPECT_EQ(Sha256Hex("Hello, World!"),
              "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f");
}

TEST(Sha256Test, DeterministicOutput) {
    const std::string input = "test_input_data";
    EXPECT_EQ(Sha256Hex(input), Sha256Hex(input));
}

TEST(Sha256Test, DifferentInputsDifferentHashes) {
    EXPECT_NE(Sha256Hex("abc"), Sha256Hex("abd"));
}

TEST(Sha256Test, OutputIs64HexChars) {
    const std::string hash = Sha256Hex("anything");
    EXPECT_EQ(hash.size(), 64u);
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(IsSha256HexTest, ValidHash) {
    EXPECT_TRUE(IsSha256Hex(Sha256Hex("test")));
}

TEST(IsSha256HexTest, TooShort) {
    EXPECT_FALSE(IsSha256Hex("abc123"));
}

TEST(IsSha256HexTest, UpperCaseNotValid) {
    std::string hash = Sha256Hex("test");
    hash[0] = 'A';
    EXPECT_FALSE(IsSha256Hex(hash));
}

TEST(IsSha256HexTest, PlaintextPassword) {
    EXPECT_FALSE(IsSha256Hex("123456"));
    EXPECT_FALSE(IsSha256Hex("mypassword"));
}
