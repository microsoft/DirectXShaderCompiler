///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxreflector.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxreflector console program.             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "dxclib/dxc.h"
#include <string>
#include <vector>

#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.h"
#include "dxc/dxcreflect.h"
#include "llvm/Support/raw_ostream.h"

#include "dxc/DXIL/DxilShaderModel.h"

inline bool wcsieq(LPCWSTR a, LPCWSTR b) { return _wcsicmp(a, b) == 0; }

using namespace dxc;
using namespace llvm::opt;
using namespace hlsl::options;

#ifdef _WIN32
int __cdecl wmain(int argc, const wchar_t **argv_) {
#else
int main(int argc, const char **argv) {
  // Convert argv to wchar.
  WArgV ArgV(argc, argv);
  const wchar_t **argv_ = ArgV.argv();
#endif
  if (FAILED(DxcInitThreadMalloc()))
    return 1;
  DxcSetThreadMallocToDefault();
  try {
    if (initHlslOptTable())
      throw std::bad_alloc();

    // Parse command line options.
    const OptTable *optionTable = getHlslOptTable();
    MainArgs argStrings(argc, argv_);
    DxcOpts dxreflectorOpts;
    DXCLibraryDllLoader dxcSupport;

    // Read options and check errors.
    {
      std::string errorString;
      llvm::raw_string_ostream errorStream(errorString);

      // Target profile is used to detect for example if 16-bit types are
      // allowed. This is the only way to correct the missing target.

      {
        unsigned missingArgIndex = 0, missingArgCount = 0;
        InputArgList Args =
            optionTable->ParseArgs(argStrings.getArrayRef(), missingArgIndex,
                                   missingArgCount, DxreflectorFlags);

        if (!Args.hasArg(OPT_target_profile)) {

          const hlsl::ShaderModel *SM = hlsl::ShaderModel::Get(
              hlsl::DXIL::ShaderKind::Library, hlsl::ShaderModel::kHighestMajor,
              hlsl::ShaderModel::kHighestMinor);

          if (SM && SM->IsValid()) {

            dxreflectorOpts.TargetProfile = SM->GetName();

            argStrings.Utf8StringVector.push_back("-T");
            argStrings.Utf8StringVector.push_back(SM->GetName());

            argStrings.Utf8CharPtrVector.resize(
                argStrings.Utf8CharPtrVector.size() + 2);

            for (uint64_t i = 0; i < argStrings.Utf8StringVector.size(); ++i)
              argStrings.Utf8CharPtrVector[i] =
                  argStrings.Utf8StringVector[i].c_str();
          }
        }
      }

      int optResult = ReadDxcOpts(optionTable, DxreflectorFlags, argStrings,
                                  dxreflectorOpts, errorStream);
      errorStream.flush();
      if (errorString.size()) {
        fprintf(stderr, "dxreflector failed : %s\n", errorString.data());
      }
      if (optResult != 0) {
        return optResult;
      }
    }

    // Apply defaults.
    if (dxreflectorOpts.EntryPoint.empty() &&
        !dxreflectorOpts.RecompileFromBinary) {
      dxreflectorOpts.EntryPoint = "main";
    }

    // Setup a helper DLL.
    {
      std::string dllErrorString;
      llvm::raw_string_ostream dllErrorStream(dllErrorString);
      int dllResult =
          SetupSpecificDllLoader(dxreflectorOpts, dxcSupport, dllErrorStream);
      dllErrorStream.flush();
      if (dllErrorString.size()) {
        fprintf(stderr, "%s\n", dllErrorString.data());
      }
      if (dllResult)
        return dllResult;
    }

    EnsureEnabled(dxcSupport);
    // Handle help request, which overrides any other processing.
    if (dxreflectorOpts.ShowHelp) {
      std::string helpString;
      llvm::raw_string_ostream helpStream(helpString);
      std::string version;
      llvm::raw_string_ostream versionStream(version);
      WriteDxCompilerVersionInfo(versionStream,
                                 dxreflectorOpts.ExternalLib.empty()
                                     ? (LPCSTR) nullptr
                                     : dxreflectorOpts.ExternalLib.data(),
                                 dxreflectorOpts.ExternalFn.empty()
                                     ? (LPCSTR) nullptr
                                     : dxreflectorOpts.ExternalFn.data(),
                                 dxcSupport);
      versionStream.flush();
      optionTable->PrintHelp(helpStream, "dxreflector.exe", "DX Reflector",
                             version.c_str(), hlsl::options::ReflectOption,
                             (dxreflectorOpts.ShowHelpHidden ? 0 : HelpHidden));
      helpStream.flush();
      WriteUtf8ToConsoleSizeT(helpString.data(), helpString.size());
      return 0;
    }

    if (dxreflectorOpts.ShowVersion) {
      std::string version;
      llvm::raw_string_ostream versionStream(version);
      WriteDxCompilerVersionInfo(versionStream,
                                 dxreflectorOpts.ExternalLib.empty()
                                     ? (LPCSTR) nullptr
                                     : dxreflectorOpts.ExternalLib.data(),
                                 dxreflectorOpts.ExternalFn.empty()
                                     ? (LPCSTR) nullptr
                                     : dxreflectorOpts.ExternalFn.data(),
                                 dxcSupport);
      versionStream.flush();
      WriteUtf8ToConsoleSizeT(version.data(), version.size());
      return 0;
    }

    CComPtr<IHLSLReflector> pReflector;
    CComPtr<IDxcResult> pReflectionResult;
    CComPtr<IDxcBlobEncoding> pSource;
    std::wstring wName(CA2W(dxreflectorOpts.InputFile.empty()
                                ? ""
                                : dxreflectorOpts.InputFile.data()));
    if (!dxreflectorOpts.InputFile.empty())
      ReadFileIntoBlob(dxcSupport, wName.c_str(), &pSource);

    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    IFT(dxcSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
    IFT(dxcSupport.CreateInstance(CLSID_DxcReflector, &pReflector));

    std::vector<std::wstring> wargs;
    std::vector<LPCWSTR> wargsC;

    wargs.reserve(argStrings.Utf8CharPtrVector.size());
    wargsC.reserve(argStrings.Utf8CharPtrVector.size());

    for (const char *arg : argStrings.Utf8CharPtrVector) {
      wargs.push_back(std::wstring(CA2W(arg)));
      wargsC.push_back(wargs.back().c_str());
    }

    IFT(pReflector->FromSource(pSource, wName.c_str(), wargsC.data(),
                               uint32_t(wargsC.size()), nullptr, 0,
                               pIncludeHandler, &pReflectionResult));

    if (dxreflectorOpts.OutputObject.empty()) {

      // No -Fo, print to console

      CComPtr<IDxcBlobEncoding> pJson;
      CComPtr<IDxcBlob> pReflectionBlob;
      CComPtr<IHLSLReflectionData> pReflectionData;

      ReflectorFormatSettings formatSettings{};
      formatSettings.PrintFileInfo = dxreflectorOpts.ReflOpt.ShowFileInfo;
      formatSettings.IsHumanReadable = !dxreflectorOpts.ReflOpt.ShowRawData;

      WriteOperationResultToConsole(pReflectionResult,
                                    !dxreflectorOpts.OutputWarnings);

      HRESULT hr;
      IFT(pReflectionResult->GetStatus(&hr));

      if (SUCCEEDED(hr)) {
        IFT(pReflectionResult->GetResult(&pReflectionBlob));
        IFT(pReflector->FromBlob(pReflectionBlob, &pReflectionData));
        IFT(pReflector->ToString(pReflectionData, formatSettings, &pJson));
        WriteBlobToConsole(pJson, STD_OUTPUT_HANDLE);
      }

    } else {
      WriteOperationErrorsToConsole(pReflectionResult,
                                    !dxreflectorOpts.OutputWarnings);
      HRESULT hr;
      IFT(pReflectionResult->GetStatus(&hr));
      if (SUCCEEDED(hr)) {
        CA2W wOutputObject(dxreflectorOpts.OutputObject.data());
        CComPtr<IDxcBlob> pObject;
        IFT(pReflectionResult->GetResult(&pObject));
        WriteBlobToFile(pObject, wOutputObject.m_psz,
                        dxreflectorOpts.DefaultTextCodePage);
      }
    }

  } catch (const ::hlsl::Exception &hlslException) {
    try {
      const char *msg = hlslException.what();
      Unicode::acp_char
          printBuffer[128]; // printBuffer is safe to treat as UTF-8 because we
                            // use ASCII contents only
      if (msg == nullptr || *msg == '\0') {
        sprintf_s(printBuffer, _countof(printBuffer),
                  "Reflection failed - error code 0x%08x.", hlslException.hr);
        msg = printBuffer;
      }

      std::string textMessage;
      bool lossy;
      if (!Unicode::UTF8ToConsoleString(msg, &textMessage, &lossy) || lossy) {
        // Do a direct assignment as a last-ditch effort and print out as UTF-8.
        textMessage = msg;
      }

      printf("%s\n", textMessage.c_str());
    } catch (...) {
      printf("Reflection failed - unable to retrieve error message.\n");
    }

    return 1;
  } catch (std::bad_alloc &) {
    printf("Reflection failed - out of memory.\n");
    return 1;
  } catch (...) {
    printf("Reflection failed - unable to retrieve error message.\n");
    return 1;
  }

  return 0;
}
