///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FileCheckerTest.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests that are based on FileChecker.                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <algorithm>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#ifdef _WIN32
#include <atlfile.h>
#endif

#include "HLSLTestData.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "D3DReflectionDumper.h"

#include "d3d12shader.h"

using namespace std;
using namespace hlsl_test;

static std::string strltrim(std::string value) {
  return value.erase(0, value.find_first_not_of(" \t\r\n"));
}

static std::string strrtrim(std::string value) {
  size_t last = value.find_last_not_of(" \t\r\n");
  return last == string::npos ? value : value.substr(0, last + 1);
}

static std::string strtrim(const std::string &value) {
  return strltrim(strrtrim(value));
}

static string trim(string value) {
  size_t leading = value.find_first_not_of(' ');
  if (leading != std::string::npos) {
    value.erase(0, leading);
  }
  size_t trailing = value.find_last_not_of(' ');
  if (leading != std::string::npos) {
    value.erase(trailing + 1);
  }
  return value;
}

    FileRunCommandPart::FileRunCommandPart(const std::string &command, const std::string &arguments, LPCWSTR commandFileName) :
      Command(command), Arguments(arguments), CommandFileName(commandFileName) { }
    FileRunCommandPart::FileRunCommandPart(FileRunCommandPart && other) :
      Command(std::move(other.Command)),
      Arguments(std::move(other.Arguments)),
      CommandFileName(other.CommandFileName),
      RunResult(other.RunResult),
      StdOut(std::move(other.StdOut)),
      StdErr(std::move(other.StdErr)) { }

    void FileRunCommandPart::Run(const FileRunCommandPart *Prior) {
      bool isFileCheck =
        0 == _stricmp(Command.c_str(), "FileCheck") ||
        0 == _stricmp(Command.c_str(), "%FileCheck");
      // For now, propagate errors.
      if (Prior && Prior->RunResult) {
        if (isFileCheck) {
          RunFileChecker(Prior);
        } else {
          StdErr = Prior->StdErr;
          RunResult = Prior->RunResult;
        }
        return;
      }

      // We would add support for 'not' and 'llc' here.
      if (isFileCheck) {
        RunFileChecker(Prior);
      }
      else if (0 == _stricmp(Command.c_str(), "StdErrCheck")) {
        RunStdErrChecker(Prior);
      }
      else if (0 == _stricmp(Command.c_str(), "tee")) {
        RunTee(Prior);
      }
      else if (0 == _stricmp(Command.c_str(), "%dxc")) {
        RunDxc(Prior);
      }
      else if (0 == _stricmp(Command.c_str(), "%dxv")) {
        RunDxv(Prior);
      }
      else if (0 == _stricmp(Command.c_str(), "%opt")) {
        RunOpt(Prior);
      }
      else if (0 == _stricmp(Command.c_str(), "%D3DReflect")) {
        RunD3DReflect(Prior);
      }
      else {
        RunResult = 1;
        StdErr = "Unrecognized command ";
        StdErr += Command;
      }
    }

    void FileRunCommandPart::RunFileChecker(const FileRunCommandPart *Prior) {
      std::string args(strtrim(Arguments));
      if (args != "%s") {
        StdErr = "Only supported pattern is a plain input file";
        RunResult = 1;
        return;
      }
      if (!Prior) {
        StdErr = "Prior command required to generate stdin";
        RunResult = 1;
        return;
      }

      CW2A fileName(CommandFileName, CP_UTF8);
      FileCheckForTest t;
      t.CheckFilename = fileName;
      if (Prior->RunResult)
        t.InputForStdin = Prior->StdErr;
      else
        t.InputForStdin = Prior->StdOut;
      RunResult = t.Run();
      StdOut = t.test_outs;
      StdErr = t.test_errs;
      // Capture the input as well.
      if (RunResult != 0 && Prior != nullptr) {
        StdErr += "\n<full input to FileCheck>\n";
        StdErr += t.InputForStdin;
      }
    }

    void FileRunCommandPart::RunStdErrChecker(const FileRunCommandPart *Prior) {
      std::string args(strtrim(Arguments));
      if (args != "%s") {
        StdErr = "Only supported pattern is a plain input file";
        RunResult = 1;
        return;
      }
      if (!Prior) {
        StdErr = "Prior command required to generate stdin";
        RunResult = 1;
        return;
      }

      CW2A fileName(CommandFileName, CP_UTF8);
      FileCheckForTest t;
      t.CheckFilename = fileName;
      
      t.InputForStdin = Prior->StdErr;
      
      RunResult = t.Run();
      StdOut = t.test_outs;
      StdErr = t.test_errs;
      // Capture the input as well.
      if (RunResult != 0 && Prior != nullptr) {
        StdErr += "\n<full input to StdErrCheck>\n";
        StdErr += t.InputForStdin;
      }
    }

    void FileRunCommandPart::ReadOptsForDxc(hlsl::options::MainArgs &argStrings,
                                            hlsl::options::DxcOpts &Opts) {
      std::string args(strtrim(Arguments));
      const char *inputPos = strstr(args.c_str(), "%s");
      if (inputPos == nullptr) {
        StdErr = "Only supported pattern includes input file as argument";
        RunResult = 1;
        return;
      }
      args.erase(inputPos - args.c_str(), strlen("%s"));

      llvm::StringRef argsRef = args;
      llvm::SmallVector<llvm::StringRef, 8> splitArgs;
      argsRef.split(splitArgs, " ");
      argStrings = hlsl::options::MainArgs(splitArgs);
      std::string errorString;
      llvm::raw_string_ostream errorStream(errorString);
      RunResult = ReadDxcOpts(hlsl::options::getHlslOptTable(), /*flagsToInclude*/ 0,
                              argStrings, Opts, errorStream);
      errorStream.flush();
      if (RunResult) {
        StdErr = errorString;
      }
    }

    void FileRunCommandPart::RunDxc(const FileRunCommandPart *Prior) {
      // Support piping stdin from prior if needed.
      UNREFERENCED_PARAMETER(Prior);
      hlsl::options::MainArgs args;
      hlsl::options::DxcOpts opts;
      ReadOptsForDxc(args, opts);

      std::wstring entry =
          Unicode::UTF8ToUTF16StringOrThrow(opts.EntryPoint.str().c_str());
      std::wstring profile =
          Unicode::UTF8ToUTF16StringOrThrow(opts.TargetProfile.str().c_str());
      std::vector<LPCWSTR> flags;
      if (opts.CodeGenHighLevel) {
        flags.push_back(L"-fcgl");
      }

      std::vector<std::wstring> argWStrings;
      CopyArgsToWStrings(opts.Args, hlsl::options::CoreOption, argWStrings);
      for (const std::wstring &a : argWStrings)
        flags.push_back(a.data());

      CComPtr<IDxcLibrary> pLibrary;
      CComPtr<IDxcCompiler> pCompiler;
      CComPtr<IDxcOperationResult> pResult;
      CComPtr<IDxcBlobEncoding> pSource;
      CComPtr<IDxcBlobEncoding> pDisassembly;
      CComPtr<IDxcBlob> pCompiledBlob;
      CComPtr<IDxcIncludeHandler> pIncludeHandler;

      HRESULT resultStatus;

      if (RunResult)  // opt parsing already failed
        return;

      IFT(DllSupport->CreateInstance(CLSID_DxcLibrary, &pLibrary));
      IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
      IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
      IFT(DllSupport->CreateInstance(CLSID_DxcCompiler, &pCompiler));
      IFT(pCompiler->Compile(pSource, CommandFileName, entry.c_str(), profile.c_str(),
                             flags.data(), flags.size(), nullptr, 0, pIncludeHandler, &pResult));
      IFT(pResult->GetStatus(&resultStatus));
      if (SUCCEEDED(resultStatus)) {
        IFT(pResult->GetResult(&pCompiledBlob));
        if (!opts.AstDump) {
          IFT(pCompiler->Disassemble(pCompiledBlob, &pDisassembly));
          StdOut = BlobToUtf8(pDisassembly);
        } else {
          StdOut = BlobToUtf8(pCompiledBlob);
        }
        CComPtr<IDxcBlobEncoding> pStdErr;
        IFT(pResult->GetErrorBuffer(&pStdErr));
        StdErr = BlobToUtf8(pStdErr);
        RunResult = 0;
      }
      else {
        IFT(pResult->GetErrorBuffer(&pDisassembly));
        StdErr = BlobToUtf8(pDisassembly);
        RunResult = resultStatus;
      }

      OpResult = pResult;
    }

    void FileRunCommandPart::RunDxv(const FileRunCommandPart *Prior) {
      std::string args(strtrim(Arguments));
      const char *inputPos = strstr(args.c_str(), "%s");
      if (inputPos == nullptr) {
        StdErr = "Only supported pattern includes input file as argument";
        RunResult = 1;
        return;
      }
      args.erase(inputPos - args.c_str(), strlen("%s"));

      llvm::StringRef argsRef = args;
      llvm::SmallVector<llvm::StringRef, 8> splitArgs;
      argsRef.split(splitArgs, " ");
      IFTMSG(splitArgs.size()==1, "wrong arg num for dxv");
      
      CComPtr<IDxcLibrary> pLibrary;
      CComPtr<IDxcAssembler> pAssembler;
      CComPtr<IDxcValidator> pValidator;
      CComPtr<IDxcOperationResult> pResult;

      CComPtr<IDxcBlobEncoding> pSource;

      CComPtr<IDxcBlob> pContainerBlob;
      HRESULT resultStatus;

      IFT(DllSupport->CreateInstance(CLSID_DxcLibrary, &pLibrary));
      IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
      IFT(DllSupport->CreateInstance(CLSID_DxcAssembler, &pAssembler));
      IFT(pAssembler->AssembleToContainer(pSource, &pResult));
      IFT(pResult->GetStatus(&resultStatus));
      if (FAILED(resultStatus)) {
        CComPtr<IDxcBlobEncoding> pAssembleBlob;
        IFT(pResult->GetErrorBuffer(&pAssembleBlob));
        StdErr = BlobToUtf8(pAssembleBlob);
        RunResult = resultStatus;
        return;
      }
      IFT(pResult->GetResult(&pContainerBlob));

      IFT(DllSupport->CreateInstance(CLSID_DxcValidator, &pValidator));
      CComPtr<IDxcOperationResult> pValidationResult;
      IFT(pValidator->Validate(pContainerBlob, DxcValidatorFlags_InPlaceEdit,
                               &pValidationResult));
      IFT(pValidationResult->GetStatus(&resultStatus));
      if (resultStatus) {
        CComPtr<IDxcBlobEncoding> pValidateBlob;
        IFT(pValidationResult->GetErrorBuffer(&pValidateBlob));
        StdOut = BlobToUtf8(pValidateBlob);
      }
      RunResult = 0;
    }

    void FileRunCommandPart::RunOpt(const FileRunCommandPart *Prior) {
      std::string args(strtrim(Arguments));
      const char *inputPos = strstr(args.c_str(), "%s");
      if (inputPos == nullptr && Prior == nullptr) {
        StdErr = "Only supported patterns are input file as argument or prior "
                 "command with disassembly";
        RunResult = 1;
        return;
      }

      CComPtr<IDxcLibrary> pLibrary;
      CComPtr<IDxcOptimizer> pOptimizer;
      CComPtr<IDxcBlobEncoding> pSource;
      CComPtr<IDxcBlobEncoding> pOutputText;
      CComPtr<IDxcBlob> pOutputModule;

      IFT(DllSupport->CreateInstance(CLSID_DxcLibrary, &pLibrary));
      IFT(DllSupport->CreateInstance(CLSID_DxcOptimizer, &pOptimizer));

      if (inputPos != nullptr) {
        args.erase(inputPos - args.c_str(), strlen("%s"));
        IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
      }
      else {
        assert(Prior != nullptr && "else early check should have returned");
        CComPtr<IDxcAssembler> pAssembler;
        IFT(DllSupport->CreateInstance(CLSID_DxcAssembler, &pAssembler));
        IFT(pLibrary->CreateBlobWithEncodingFromPinned(
            Prior->StdOut.c_str(), Prior->StdOut.size(), CP_UTF8,
            &pSource));
      }

      args = trim(args);
      llvm::StringRef argsRef = args;
      llvm::SmallVector<llvm::StringRef, 8> splitArgs;
      argsRef.split(splitArgs, " ");
      std::vector<LPCWSTR> options;
      std::vector<std::wstring> optionStrings;
      for (llvm::StringRef S : splitArgs) {
        optionStrings.push_back(
            Unicode::UTF8ToUTF16StringOrThrow(trim(S.str()).c_str()));
      }

      // Add the options outside the above loop in case the vector is resized.
      for (const std::wstring& str : optionStrings)
        options.push_back(str.c_str());

      IFT(pOptimizer->RunOptimizer(pSource, options.data(), options.size(),
                                   &pOutputModule, &pOutputText));
      StdOut = BlobToUtf8(pOutputText);
      RunResult = 0;
    }

    void FileRunCommandPart::RunD3DReflect(const FileRunCommandPart *Prior) {
      std::string args(strtrim(Arguments));
      if (args != "%s") {
        StdErr = "Only supported pattern is a plain input file";
        RunResult = 1;
        return;
      }
      if (!Prior) {
        StdErr = "Prior command required to generate stdin";
        RunResult = 1;
        return;
      }

      CComPtr<IDxcLibrary> pLibrary;
      CComPtr<IDxcBlobEncoding> pSource;
      CComPtr<IDxcAssembler> pAssembler;
      CComPtr<IDxcOperationResult> pResult;
      CComPtr<ID3D12ShaderReflection> pShaderReflection;
      CComPtr<ID3D12LibraryReflection> pLibraryReflection;
      CComPtr<IDxcContainerReflection> containerReflection;
      uint32_t partCount;
      CComPtr<IDxcBlob> pContainerBlob;
      HRESULT resultStatus;
      bool blobFound = false;
      std::ostringstream ss;
      D3DReflectionDumper dumper(ss);

      IFT(DllSupport->CreateInstance(CLSID_DxcLibrary, &pLibrary));
      IFT(DllSupport->CreateInstance(CLSID_DxcAssembler, &pAssembler));

      IFT(pLibrary->CreateBlobWithEncodingFromPinned(
          (LPBYTE)Prior->StdOut.c_str(), Prior->StdOut.size(), CP_UTF8,
          &pSource));

      IFT(pAssembler->AssembleToContainer(pSource, &pResult));
      IFT(pResult->GetStatus(&resultStatus));
      if (FAILED(resultStatus)) {
        CComPtr<IDxcBlobEncoding> pAssembleBlob;
        IFT(pResult->GetErrorBuffer(&pAssembleBlob));
        StdErr = BlobToUtf8(pAssembleBlob);
        RunResult = resultStatus;
        return;
      }
      IFT(pResult->GetResult(&pContainerBlob));

      VERIFY_SUCCEEDED(DllSupport->CreateInstance(CLSID_DxcContainerReflection, &containerReflection));
      VERIFY_SUCCEEDED(containerReflection->Load(pContainerBlob));
      VERIFY_SUCCEEDED(containerReflection->GetPartCount(&partCount));

      for (uint32_t i = 0; i < partCount; ++i) {
        uint32_t kind;
        VERIFY_SUCCEEDED(containerReflection->GetPartKind(i, &kind));
        if (kind == (uint32_t)hlsl::DxilFourCC::DFCC_DXIL) {
          blobFound = true;
          CComPtr<IDxcBlob> pPart;
          IFT(containerReflection->GetPartContent(i, &pPart));
          const hlsl::DxilProgramHeader *pProgramHeader =
            reinterpret_cast<const hlsl::DxilProgramHeader*>(pPart->GetBufferPointer());
          VERIFY_IS_TRUE(IsValidDxilProgramHeader(pProgramHeader, (uint32_t)pPart->GetBufferSize()));
          hlsl::DXIL::ShaderKind SK = hlsl::GetVersionShaderType(pProgramHeader->ProgramVersion);
          if (SK == hlsl::DXIL::ShaderKind::Library)
            VERIFY_SUCCEEDED(containerReflection->GetPartReflection(i, IID_PPV_ARGS(&pLibraryReflection)));
          else
            VERIFY_SUCCEEDED(containerReflection->GetPartReflection(i, IID_PPV_ARGS(&pShaderReflection)));
          break;
        }
      }

      if (!blobFound) {
        StdErr = "Unable to find DXIL part";
        RunResult = 1;
        return;
      } else if (pShaderReflection) {
        dumper.Dump(pShaderReflection);
      } else if (pLibraryReflection) {
        dumper.Dump(pLibraryReflection);
      }

      ss.flush();
      StdOut = ss.str();
      RunResult = 0;
    }

    void FileRunCommandPart::RunTee(const FileRunCommandPart *Prior) {
      if (Prior == nullptr) {
        StdErr = "tee requires a prior command";
        RunResult = 1;
        return;
      }

      // Ignore commands for now - simply log out through test framework.
      {
        CA2W outWide(Prior->StdOut.c_str(), CP_UTF8);
        WEX::Logging::Log::Comment(outWide.m_psz);
      }
      if (!Prior->StdErr.empty()) {
        CA2W errWide(Prior->StdErr.c_str(), CP_UTF8);
        WEX::Logging::Log::Comment(L"<stderr>");
        WEX::Logging::Log::Comment(errWide.m_psz);
      }

      StdErr = Prior->StdErr;
      StdOut = Prior->StdOut;
      RunResult = Prior->RunResult;
    }

class FileRunTestResultImpl : public FileRunTestResult {
  dxc::DxcDllSupport &m_support;

  void RunFileCheckFromCommands(LPCSTR commands, LPCWSTR fileName) {
    std::vector<FileRunCommandPart> parts;
    ParseCommandParts(commands, fileName, parts);
    FileRunCommandPart *prior = nullptr;
    for (FileRunCommandPart & part : parts) {
      part.DllSupport = &m_support;
      part.Run(prior);
      prior = &part;
    }
    if (prior == nullptr) {
      this->RunResult = 1;
      this->ErrorMessage = "FileCheck found no commands to run";
    }
    else {
      this->RunResult = prior->RunResult;
      this->ErrorMessage = prior->StdErr;
    }
  }

public:
  FileRunTestResultImpl(dxc::DxcDllSupport &support) : m_support(support) {}
  void RunFileCheckFromFileCommands(LPCWSTR fileName) {
    // Assume UTF-8 files.
    std::string commands(GetFirstLine(fileName));
    return RunFileCheckFromCommands(commands.c_str(), fileName);
  }
};

FileRunTestResult FileRunTestResult::RunFromFileCommands(LPCWSTR fileName) {
  dxc::DxcDllSupport dllSupport;
  IFT(dllSupport.Initialize());
  FileRunTestResultImpl result(dllSupport);
  result.RunFileCheckFromFileCommands(fileName);
  return result;
}

FileRunTestResult FileRunTestResult::RunFromFileCommands(LPCWSTR fileName, dxc::DxcDllSupport &dllSupport) {
  FileRunTestResultImpl result(dllSupport);
  result.RunFileCheckFromFileCommands(fileName);
  return result;
}

void ParseCommandParts(LPCSTR commands, LPCWSTR fileName,
                       std::vector<FileRunCommandPart> &parts) {
  // Barely enough parsing here.
  commands = strstr(commands, "RUN: ");
  if (!commands) {
    return;
  }
  commands += strlen("RUN: ");

  LPCSTR endCommands = strchr(commands, '\0');
  while (commands != endCommands) {
    LPCSTR nextStart;
    LPCSTR thisEnd = strchr(commands, '|');
    if (!thisEnd) {
      nextStart = thisEnd = endCommands;
    } else {
      nextStart = thisEnd + 2;
    }
    LPCSTR commandEnd = strchr(commands, ' ');
    if (!commandEnd)
      commandEnd = endCommands;
    parts.emplace_back(std::string(commands, commandEnd),
                       std::string(commandEnd, thisEnd), fileName);
    commands = nextStart;
  }
}

void ParseCommandPartsFromFile(LPCWSTR fileName,
                               std::vector<FileRunCommandPart> &parts) {
  // Assume UTF-8 files.
  std::string commands(GetFirstLine(fileName));
  ParseCommandParts(commands.c_str(), fileName, parts);
}
