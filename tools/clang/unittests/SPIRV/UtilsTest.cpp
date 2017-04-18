//===- unittests/SPIRV/UtilsTest.cpp ------ Utils tests -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "clang/SPIRV/Utils.h"
#include "gtest/gtest.h"

namespace {

using namespace clang::spirv;
using ::testing::ElementsAre;

TEST(Utils, EncodeEmptyString) {
  std::string str = "";
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);
  EXPECT_THAT(words, ElementsAre(0u));
}
TEST(Utils, EncodeOneCharString) {
  std::string str = "m";
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);
  EXPECT_THAT(words, ElementsAre(109u));
}
TEST(Utils, EncodeTwoCharString) {
  std::string str = "ma";
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);
  EXPECT_THAT(words, ElementsAre(24941u));
}
TEST(Utils, EncodeThreeCharString) {
  std::string str = "mai";
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);
  EXPECT_THAT(words, ElementsAre(6906221u));
}
TEST(Utils, EncodeFourCharString) {
  std::string str = "main";
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);
  EXPECT_THAT(words, ElementsAre(1852399981u, 0u));
}
TEST(Utils, EncodeString) {
  // Bin  01110100   01110011    01100101    01010100 = unsigned(1,953,719,636)
  // Hex     74         73          65          54
  //          t          s           e           T
  // Bin  01101001   01110010    01110100    01010011 =  unsigned(1,769,108,563)
  // Hex     69         72          74          53
  //          i          r           t           S
  // Bin  00000000   00000000    01100111    01101110 =  unsigned(26,478)
  // Hex      0          0          67          6E
  //          \0         \0          g           n
  std::string str = "TestString";
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);
  EXPECT_THAT(words, ElementsAre(1953719636, 1769108563, 26478));
}
TEST(Utils, DecodeString) {
  // Bin  01110100   01110011    01100101    01010100 = unsigned(1,953,719,636)
  // Hex     74         73          65          54
  //          t          s           e           T
  // Bin  01101001   01110010    01110100    01010011 =  unsigned(1,769,108,563)
  // Hex     69         72          74          53
  //          i          r           t           S
  // Bin  00000000   00000000    01100111    01101110 =  unsigned(26,478)
  // Hex      0          0          67          6E
  //          \0         \0          g           n
  std::vector<uint32_t> words = {1953719636, 1769108563, 26478};
  std::string str = utils::decodeSPIRVString(words);
  EXPECT_EQ(str, "TestString");
}
TEST(Utils, EncodeAndDecodeString) {
  std::string str = "TestString";
  // Convert to vector
  std::vector<uint32_t> words = utils::encodeSPIRVString(str);

  // Convert back to string
  std::string result = utils::decodeSPIRVString(words);

  EXPECT_EQ(str, result);
}

// TODO: Add more ModuleBuilder tests

} // anonymous namespace
