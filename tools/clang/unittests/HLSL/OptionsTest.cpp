///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// OptionsTest.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the command-line options APIs.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"

#include "dxc/Test/HLSLTestData.h"
#ifdef _WIN32
#include "WexTestClass.h"
#endif
#include "dxc/Test/HlslTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/ADT/STLExtras.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"

#include <fstream>

using namespace std;
using namespace hlsl_test;
using namespace hlsl::options;

/// Use this class to construct MainArgs from constants. Handy to use because
/// DxcOpts will StringRef into it.
class MainArgsArr : public MainArgs {
public:
  template <size_t n>
  MainArgsArr(const wchar_t *(&arr)[n]) : MainArgs(n, arr) {}
};

#ifdef _WIN32
class OptionsTest {
#else
class OptionsTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(OptionsTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(ReadOptionsWhenDefinesThenInit)
  TEST_METHOD(ReadOptionsWhenExtensionsThenOK)
  TEST_METHOD(ReadOptionsWhenHelpThenShortcut)
  TEST_METHOD(ReadOptionsWhenInvalidThenFail)
  TEST_METHOD(ReadOptionsConflict)
  TEST_METHOD(ReadOptionsWhenValidThenOK)
  TEST_METHOD(ReadOptionsWhenJoinedThenOK)
  TEST_METHOD(ReadOptionsWhenNoEntryThenOK)
  TEST_METHOD(ReadOptionsForOutputObject)

  TEST_METHOD(ReadOptionsForDxcWhenApiArgMissingThenFail)
  TEST_METHOD(ReadOptionsForApiWhenApiArgMissingThenOK)

  TEST_METHOD(ConvertWhenFailThenThrow)

  TEST_METHOD(CopyOptionsWhenSingleThenOK)
  //TEST_METHOD(CopyOptionsWhenMultipleThenOK)

  TEST_METHOD(ReadOptionsJoinedWithSpacesThenOK)

  std::unique_ptr<DxcOpts> ReadOptsTest(const MainArgs &mainArgs,
                                        unsigned flagsToInclude,
                                        bool shouldFail = false,
                                        bool shouldMessage = false) {
    std::string errorString;
    llvm::raw_string_ostream errorStream(errorString);
    std::unique_ptr<DxcOpts> opts = llvm::make_unique<DxcOpts>();
    int result = ReadDxcOpts(getHlslOptTable(), flagsToInclude, mainArgs,
                             *(opts.get()), errorStream);
    EXPECT_EQ(shouldFail, result != 0);
    EXPECT_EQ(shouldMessage, !errorStream.str().empty());
    return opts;
  }
  void ReadOptsTest(const MainArgs &mainArgs,
                                        unsigned flagsToInclude,
      const char *expectErrorMsg) {
    std::string errorString;
    llvm::raw_string_ostream errorStream(errorString);
    std::unique_ptr<DxcOpts> opts = llvm::make_unique<DxcOpts>();
    int result = ReadDxcOpts(getHlslOptTable(), flagsToInclude, mainArgs,
                             *(opts.get()), errorStream);
    EXPECT_EQ(result, 1);
    VERIFY_ARE_EQUAL_STR(expectErrorMsg, errorStream.str().c_str());
  }
};

TEST_F(OptionsTest, ReadOptionsWhenExtensionsThenOK) {
  const wchar_t *Args[] = {
      L"exe.exe",   L"/E",        L"main",    L"/T",           L"ps_6_0",
      L"hlsl.hlsl", L"-external", L"foo.dll", L"-external-fn", L"CreateObj"};
  const wchar_t *ArgsNoLib[] = {
    L"exe.exe",   L"/E",        L"main",    L"/T",           L"ps_6_0",
    L"hlsl.hlsl", L"-external-fn", L"CreateObj" };
  const wchar_t *ArgsNoFn[] = {
    L"exe.exe",   L"/E",        L"main",    L"/T",           L"ps_6_0",
    L"hlsl.hlsl", L"-external", L"foo.dll" };
  MainArgsArr ArgsArr(Args);
  std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
  VERIFY_ARE_EQUAL_STR("CreateObj", o->ExternalFn.data());
  VERIFY_ARE_EQUAL_STR("foo.dll", o->ExternalLib.data());

  MainArgsArr ArgsNoLibArr(ArgsNoLib);
  ReadOptsTest(ArgsNoLibArr, DxcFlags, true, true);
  MainArgsArr ArgsNoFnArr(ArgsNoFn);
  ReadOptsTest(ArgsNoFnArr, DxcFlags, true, true);
}

TEST_F(OptionsTest, ReadOptionsForOutputObject) {
  const wchar_t *Args[] = {
      L"exe.exe",   L"/E",        L"main",    L"/T",           L"ps_6_0",
      L"hlsl.hlsl", L"-Fo", L"hlsl.dxbc"};
  MainArgsArr ArgsArr(Args);
  std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
  VERIFY_ARE_EQUAL_STR("hlsl.dxbc", o->OutputObject.data());  
}

TEST_F(OptionsTest, ReadOptionsConflict) {
  const wchar_t *matrixArgs[] = {
      L"exe.exe",   L"/E",        L"main",    L"/T",           L"ps_6_0",
      L"-Zpr", L"-Zpc",
      L"hlsl.hlsl"};
  MainArgsArr ArgsArr(matrixArgs);
  ReadOptsTest(ArgsArr, DxcFlags, "Cannot specify /Zpr and /Zpc together, use /? to get usage information");

  const wchar_t *controlFlowArgs[] = {
      L"exe.exe",   L"/E",        L"main",    L"/T",           L"ps_6_0",
      L"-Gfa", L"-Gfp",
      L"hlsl.hlsl"};
  MainArgsArr controlFlowArr(controlFlowArgs);
  ReadOptsTest(controlFlowArr, DxcFlags, "Cannot specify /Gfa and /Gfp together, use /? to get usage information");

  const wchar_t *libArgs[] = {
      L"exe.exe",   L"/E",        L"main",    L"/T",           L"lib_6_1",
      L"hlsl.hlsl"};
  MainArgsArr libArr(libArgs);
  ReadOptsTest(libArr, DxcFlags, "Must disable validation for unsupported lib_6_1 or lib_6_2 targets.");
}

TEST_F(OptionsTest, ReadOptionsWhenHelpThenShortcut) {
  const wchar_t *Args[] = { L"exe.exe", L"--help", L"--unknown-flag" };
  MainArgsArr ArgsArr(Args);
  std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
  EXPECT_EQ(true, o->ShowHelp);
}

TEST_F(OptionsTest, ReadOptionsWhenValidThenOK) {
  const wchar_t *Args[] = { L"exe.exe", L"/E", L"main", L"/T", L"ps_6_0", L"hlsl.hlsl" };
  MainArgsArr ArgsArr(Args);
  std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
  VERIFY_ARE_EQUAL_STR("main", o->EntryPoint.data());
  VERIFY_ARE_EQUAL_STR("ps_6_0", o->TargetProfile.data());
  VERIFY_ARE_EQUAL_STR("hlsl.hlsl", o->InputFile.data());
}

TEST_F(OptionsTest, ReadOptionsWhenJoinedThenOK) {
  const wchar_t *Args[] = { L"exe.exe", L"/Emain", L"/Tps_6_0", L"hlsl.hlsl" };
  MainArgsArr ArgsArr(Args);
  std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
  VERIFY_ARE_EQUAL_STR("main", o->EntryPoint.data());
  VERIFY_ARE_EQUAL_STR("ps_6_0", o->TargetProfile.data());
  VERIFY_ARE_EQUAL_STR("hlsl.hlsl", o->InputFile.data());
}

TEST_F(OptionsTest, ReadOptionsWhenNoEntryThenOK) {
  // It's not an error to omit the entry function name, but it's not
  // set to 'main' on behalf of callers either.
  const wchar_t *Args[] = { L"exe.exe", L"/T", L"ps_6_0", L"hlsl.hlsl" };
  MainArgsArr ArgsArr(Args);
  std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
  VERIFY_IS_TRUE(o->EntryPoint.empty());
}

TEST_F(OptionsTest, ReadOptionsWhenInvalidThenFail) {
  const wchar_t *ArgsNoTarget[] = {L"exe.exe", L"/E", L"main", L"hlsl.hlsl"};
  const wchar_t *ArgsNoInput[] = {L"exe.exe", L"/E", L"main", L"/T", L"ps_6_0"};
  const wchar_t *ArgsNoArg[] = {L"exe.exe", L"hlsl.hlsl", L"/E", L"main",
                                L"/T"};
  const wchar_t *ArgsUnknown[] = { L"exe.exe", L"hlsl.hlsl", L"/E", L"main",
    L"/T" L"ps_6_0", L"--unknown"};
  const wchar_t *ArgsUnknownButIgnore[] = { L"exe.exe", L"hlsl.hlsl", L"/E", L"main",
    L"/T", L"ps_6_0", L"--unknown", L"-Qunused-arguments" };
  MainArgsArr ArgsNoTargetArr(ArgsNoTarget),
      ArgsNoInputArr(ArgsNoInput), ArgsNoArgArr(ArgsNoArg),
    ArgsUnknownArr(ArgsUnknown), ArgsUnknownButIgnoreArr(ArgsUnknownButIgnore);
  ReadOptsTest(ArgsNoTargetArr, DxcFlags, true, true);
  ReadOptsTest(ArgsNoInputArr, DxcFlags, true, true);
  ReadOptsTest(ArgsNoArgArr, DxcFlags, true, true);
  ReadOptsTest(ArgsUnknownArr, DxcFlags, true, true);
  ReadOptsTest(ArgsUnknownButIgnoreArr, DxcFlags);
}

TEST_F(OptionsTest, ReadOptionsWhenDefinesThenInit) {
  const wchar_t *ArgsNoDefines[] = { L"exe.exe", L"/T", L"ps_6_0", L"/E", L"main", L"hlsl.hlsl" };
  const wchar_t *ArgsOneDefine[] = { L"exe.exe", L"/DNAME1=1", L"/T", L"ps_6_0", L"/E", L"main", L"hlsl.hlsl" };
  const wchar_t *ArgsTwoDefines[] = { L"exe.exe", L"/DNAME1=1", L"/T", L"ps_6_0", L"/D", L"NAME2=2", L"/E", L"main", L"/T", L"ps_6_0", L"hlsl.hlsl"};
  const wchar_t *ArgsEmptyDefine[] = { L"exe.exe", L"/DNAME1", L"hlsl.hlsl", L"/E", L"main", L"/T", L"ps_6_0", };

  MainArgsArr ArgsNoDefinesArr(ArgsNoDefines), ArgsOneDefineArr(ArgsOneDefine),
      ArgsTwoDefinesArr(ArgsTwoDefines), ArgsEmptyDefineArr(ArgsEmptyDefine);

  std::unique_ptr<DxcOpts> o;
  o = ReadOptsTest(ArgsNoDefinesArr, DxcFlags);
  EXPECT_EQ(0U, o->Defines.size());
  
  o = ReadOptsTest(ArgsOneDefineArr, DxcFlags);
  EXPECT_EQ(1U, o->Defines.size());
  EXPECT_STREQW(L"NAME1", o->Defines.data()[0].Name);
  EXPECT_STREQW(L"1", o->Defines.data()[0].Value);

  o = ReadOptsTest(ArgsTwoDefinesArr, DxcFlags);
  EXPECT_EQ(2U, o->Defines.size());
  EXPECT_STREQW(L"NAME1", o->Defines.data()[0].Name);
  EXPECT_STREQW(L"1", o->Defines.data()[0].Value);
  EXPECT_STREQW(L"NAME2", o->Defines.data()[1].Name);
  EXPECT_STREQW(L"2", o->Defines.data()[1].Value);

  o = ReadOptsTest(ArgsEmptyDefineArr, DxcFlags);
  EXPECT_EQ(1U, o->Defines.size());
  EXPECT_STREQW(L"NAME1", o->Defines.data()[0].Name);
  EXPECT_EQ(nullptr, o->Defines.data()[0].Value);
}

TEST_F(OptionsTest, ReadOptionsForDxcWhenApiArgMissingThenFail) {
  // When an argument specified through an API argument is not specified (eg the
  // target model), for the command-line dxc.exe tool, then the validation should
  // fail.
  const wchar_t *Args[] = {L"exe.exe", L"/E", L"main", L"hlsl.hlsl"};

  MainArgsArr mainArgsArr(Args);

  std::unique_ptr<DxcOpts> o;
  o = ReadOptsTest(mainArgsArr, DxcFlags, true, true);
}

TEST_F(OptionsTest, ReadOptionsForApiWhenApiArgMissingThenOK) {
  // When an argument specified through an API argument is not specified (eg the
  // target model), for an API, then the validation should not fail.
  const wchar_t *Args[] = { L"exe.exe", L"/E", L"main", L"hlsl.hlsl" };

  MainArgsArr mainArgsArr(Args);

  std::unique_ptr<DxcOpts> o;
  o = ReadOptsTest(mainArgsArr, CompilerFlags, false, false);
}


TEST_F(OptionsTest, ConvertWhenFailThenThrow) {
  std::wstring utf16;

  // Simple test to verify conversion works.
  EXPECT_EQ(true, Unicode::UTF8ToUTF16String("test", &utf16));
  EXPECT_STREQW(L"test", utf16.data());

  // Simple test to verify conversion works with actual UTF-8 and not just ASCII.
  // n with tilde is Unicode 0x00F1, encoded in UTF-8 as 0xC3 0xB1
  EXPECT_EQ(true, Unicode::UTF8ToUTF16String("\xC3\xB1", &utf16));
  EXPECT_STREQW(L"\x00F1", utf16.data());

  // Fail when the sequence is incomplete.
  EXPECT_EQ(false, Unicode::UTF8ToUTF16String("\xC3", &utf16));

  // Throw on failure.
  bool thrown = false;
  try {
    Unicode::UTF8ToUTF16StringOrThrow("\xC3");
  }
  catch (...) {
    thrown = true;
  }
  EXPECT_EQ(true, thrown);
}

TEST_F(OptionsTest, CopyOptionsWhenSingleThenOK) {
  const char *ArgsNoDefines[] = {"/T",   "ps_6_0",    "/E",
                                 "main", "hlsl.hlsl", "-unknown"};
  const llvm::opt::OptTable *table = getHlslOptTable();
  unsigned missingIndex = 0, missingArgCount = 0;
  llvm::opt::InputArgList args =
      table->ParseArgs(ArgsNoDefines, missingIndex, missingArgCount, DxcFlags);
  std::vector<std::wstring> outArgs;
  CopyArgsToWStrings(args, DxcFlags, outArgs);
  EXPECT_EQ(4U, outArgs.size()); // -unknown and hlsl.hlsl are missing
  VERIFY_ARE_NOT_EQUAL(outArgs.end(), std::find(outArgs.begin(), outArgs.end(), std::wstring(L"/T")));
  VERIFY_ARE_NOT_EQUAL(outArgs.end(), std::find(outArgs.begin(), outArgs.end(), std::wstring(L"ps_6_0")));
  VERIFY_ARE_NOT_EQUAL(outArgs.end(), std::find(outArgs.begin(), outArgs.end(), std::wstring(L"/E")));
  VERIFY_ARE_NOT_EQUAL(outArgs.end(), std::find(outArgs.begin(), outArgs.end(), std::wstring(L"main")));
  VERIFY_ARE_EQUAL    (outArgs.end(), std::find(outArgs.begin(), outArgs.end(), std::wstring(L"hlsl.hlsl")));
}

TEST_F(OptionsTest, ReadOptionsJoinedWithSpacesThenOK) {
  {
    // Ensure parsing arguments in joined form with embedded spaces
    // between the option and the argument works, for these argument types:
    // - JoinedOrSeparateClass (-E, -T)
    // - SeparateClass (-external, -external-fn)
    const wchar_t *Args[] = {
      L"exe.exe",   L"-E main",    L"/T  ps_6_0",
      L"hlsl.hlsl", L"-external foo.dll", L"-external-fn  CreateObj"};
    MainArgsArr ArgsArr(Args);
    std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
    VERIFY_ARE_EQUAL_STR("main", o->EntryPoint.data());
    VERIFY_ARE_EQUAL_STR("ps_6_0", o->TargetProfile.data());
    VERIFY_ARE_EQUAL_STR("CreateObj", o->ExternalFn.data());
    VERIFY_ARE_EQUAL_STR("foo.dll", o->ExternalLib.data());
  }

  {
    // Ignore trailing spaces in option name for JoinedOrSeparateClass
    // Otherwise error messages are not easy for user to interpret
    const wchar_t *Args[] = {
      L"exe.exe",   L"-E ", L"main",    L"/T  ", L"ps_6_0",
      L"hlsl.hlsl"};
    MainArgsArr ArgsArr(Args);
    std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
    VERIFY_ARE_EQUAL_STR("main", o->EntryPoint.data());
    VERIFY_ARE_EQUAL_STR("ps_6_0", o->TargetProfile.data());
  }
  {
    // Ignore trailing spaces in option name for SeparateClass
    // Otherwise error messages are not easy for user to interpret
    const wchar_t *Args[] = {
      L"exe.exe",   L"-E", L"main",    L"/T", L"ps_6_0",
      L"hlsl.hlsl", L"-external ", L"foo.dll", L"-external-fn  ", L"CreateObj"};
    MainArgsArr ArgsArr(Args);
    std::unique_ptr<DxcOpts> o = ReadOptsTest(ArgsArr, DxcFlags);
    VERIFY_ARE_EQUAL_STR("CreateObj", o->ExternalFn.data());
    VERIFY_ARE_EQUAL_STR("foo.dll", o->ExternalLib.data());
  }
}
