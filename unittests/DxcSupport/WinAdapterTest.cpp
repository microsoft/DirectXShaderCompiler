//===- unittests/Basic/WinAdapterTest.cpp -- Windows Adapter tests --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _WIN32

// Include the upstream adapter first to exercise its UUID emulation.
#include <wsl/winadapter.h>

#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/WinIncludes.h"
#include "gtest/gtest.h"

#include <string.h>

CROSS_PLATFORM_UUIDOF(WinAdapterTestInterface,
                      "12345678-9abc-def0-1234-56789abcdef0")
struct WinAdapterTestInterface : public IUnknown {};

namespace {

TEST(WinAdapterTest, InterfaceUuid) {
  const GUID expected = {0x12345678,
                         0x9abc,
                         0xdef0,
                         {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}};
  const GUID actual = __uuidof(WinAdapterTestInterface);
  EXPECT_EQ(0, memcmp(&expected, &actual, sizeof(GUID)));
  WinAdapterTestInterface *object = nullptr;
  const GUID *iid = nullptr;
  void **address = nullptr;
  const auto capture = [&](REFIID value, void **pointer) {
    iid = &value;
    address = pointer;
  };
  capture(IID_PPV_ARGS(&object));
  EXPECT_EQ(0, memcmp(&expected, iid, sizeof(GUID)));
  EXPECT_EQ(reinterpret_cast<void **>(&object), address);
}

TEST(WinAdapterTest, UnknownQueryInterface) {
  class TestObject : public WinAdapterTestInterface {
  public:
    HRESULT QueryInterface(REFIID iid, void **object) override {
      *object = nullptr;
      if (!IsEqualIID(iid, __uuidof(WinAdapterTestInterface)) &&
          !IsEqualIID(iid, __uuidof(IUnknown)))
        return E_NOINTERFACE;
      *object = static_cast<WinAdapterTestInterface *>(this);
      AddRef();
      return S_OK;
    }
    ULONG AddRef() override { return ++References; }
    ULONG Release() override { return --References; }
    ULONG References = 1;
  };
  TestObject object;
  IUnknown *unknown = &object;
  WinAdapterTestInterface *queried = nullptr;
  ASSERT_EQ(S_OK, unknown->QueryInterface(&queried));
  ASSERT_EQ(&object, queried);
  EXPECT_EQ(2u, object.References);
  EXPECT_EQ(1u, queried->Release());
  const GUID expected = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
  EXPECT_TRUE(IsEqualIID(expected, __uuidof(IUnknown)));
}

TEST(WinAdapterTest, WideToConsoleStringReportsLoss) {
  std::string encoded;
  bool lossy = true;
  EXPECT_TRUE(Unicode::WideToConsoleString(L"DXC", 3, &encoded, &lossy));
  EXPECT_EQ("DXC", encoded);
  EXPECT_FALSE(lossy);
}

// Check that WArgV.
TEST(WArgVTest, suppressAndTrap) {
  int argc = 2;
  std::wstring data[] = {L"a", L"b"};
  const wchar_t *ref_argv[] = {data[0].c_str(), data[1].c_str()};
  {
    const char *argv[] = {"a", "b"};
    WArgV ArgV(argc, argv);
    const wchar_t **wargv = ArgV.argv();
    for (int i = 0; i < argc; ++i) {
      EXPECT_EQ(0, std::wcscmp(ref_argv[i], wargv[i]));
    }
  }
}

} // namespace
#endif
