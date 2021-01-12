///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxilpdbutils.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements IDxcPdbUtils interface, which allows getting basic stuff from  //
// shader PDBS.                                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/FileIOHelper.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"

#include "dxc/dxcapi.h"
#include "dxc/dxcpix.h"
#include "dxc/Support/microcom.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DXIL/DxilPDB.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/Support/Unicode.h"

#include "dxcshadersourceinfo.h"

#include <vector>
#include <locale>
#include <codecvt>
#include <string>
#include <dia2.h>

using namespace dxc;
using namespace llvm;

static std::wstring ToWstring(const char *ptr, size_t size) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
  return converter.from_bytes(ptr, ptr+size);
}
static std::wstring ToWstring(const char *ptr) {
  return ToWstring(ptr, strlen(ptr));
}
static std::wstring ToWstring(const std::string &str) {
  return ToWstring(str.data(), str.size());
}
static std::wstring ToWstring(StringRef str) {
  return ToWstring(str.data(), str.size());
}
static HRESULT CopyWstringToBSTR(const std::wstring &str, BSTR *pResult) {
  if (!pResult) return E_POINTER;
  *pResult = nullptr;

  if (str.empty())
    return S_OK;

  CComBSTR buf(str.data());
  *pResult = buf.Detach();
  return S_OK;
}

static std::wstring NormalizePath(const WCHAR *path) {
  std::string FilenameStr8 = Unicode::UTF16ToUTF8StringOrThrow(path);
  llvm::SmallString<128> NormalizedPath;
  llvm::sys::path::native(FilenameStr8, NormalizedPath);
  std::wstring FilenameStr16 = Unicode::UTF8ToUTF16StringOrThrow(NormalizedPath.c_str());
  return FilenameStr16;
}

static bool IsBitcode(const void *ptr, size_t size) {
  const uint8_t pattern[] = {'B','C',};
  if (size < _countof(pattern))
    return false;
  return !memcmp(ptr, pattern, _countof(pattern));
}

static bool ShouldIncludeInFlags(const std::wstring &str, bool *skip_next_arg) {
  *skip_next_arg = false;
  const wchar_t *specialCases[] = { L"/T", L"-T", L"-D", L"/D", L"-E", L"/E", };
  for (unsigned i = 0; i < _countof(specialCases); i++) {
    if (str == specialCases[i]) {
      *skip_next_arg = true;
      return false;
    }
    else if (str.find(specialCases[i]) == 0) {
      return false;
    }
  }
  return true;
}

static std::vector<std::wstring> ComputeFlagsBasedOnArgs(ArrayRef<std::wstring> args) {
  std::vector<std::wstring> flags;
  // Flags - which exclude entry point, target profile, and defines
  for (unsigned i = 0; i < args.size(); i++) {
    const std::wstring &arg = args[i];
    bool skip_another_arg = false;
    if (ShouldIncludeInFlags(arg, &skip_another_arg)) {
      flags.push_back(arg);
    }
    if (skip_another_arg)
      i++;
  }
  return flags;
}

struct DxcPdbVersionInfo : public IDxcVersionInfo2 {
private:
  DXC_MICROCOM_TM_REF_FIELDS()

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(DxcPdbVersionInfo)

  DxcPdbVersionInfo(IMalloc *pMalloc) : m_dwRef(0), m_pMalloc(pMalloc) {}

  hlsl::DxilCompilerVersion m_Version = {};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcVersionInfo, IDxcVersionInfo2>(this, iid, ppvObject);
  }

  virtual HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor) override {
    if (!pMajor || !pMinor)
      return E_POINTER;
    *pMajor = m_Version.Major;
    *pMinor = m_Version.Minor;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags) {
    if (!pFlags) return E_POINTER;
    *pFlags = m_Version.VersionFlags;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetCommitInfo(_Out_ UINT32 *pCommitCount, _Out_ char **pCommitHash) {
    if (!pCommitHash)
      return E_POINTER;

    *pCommitHash = nullptr;

    char *const hash = (char *)CoTaskMemAlloc(sizeof(m_Version.CommitSha) + 1);
    if (hash == nullptr)
      return E_OUTOFMEMORY;
    std::memcpy(hash, m_Version.CommitSha, sizeof(m_Version.CommitSha));
    hash[sizeof(m_Version.CommitSha)] = 0;

    *pCommitHash = hash;
    *pCommitCount = m_Version.CommitCount;

    return S_OK;
  }
};

struct PdbRecompilerIncludeHandler : public IDxcIncludeHandler {
  private:
  DXC_MICROCOM_TM_REF_FIELDS()

  public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(PdbRecompilerIncludeHandler)

  PdbRecompilerIncludeHandler(IMalloc *pMalloc) : m_dwRef(0), m_pMalloc(pMalloc) {}

  std::unordered_map< std::wstring, unsigned > m_FileMap;
  IDxcPdbUtils *m_pPdbUtils = nullptr;

  UINT32 m_Major = 0;
  UINT32 m_Minor = 0;
  std::string m_Hash;
  UINT32 m_Flags = 0;
  UINT32 m_CommitCount = 0;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, iid, ppvObject);
  }

  virtual HRESULT STDMETHODCALLTYPE LoadSource(
    _In_z_ LPCWSTR pFilename,                                 // Candidate filename.
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource  // Resultant source object for included file, nullptr if not found.
    ) override
  {
    auto it = m_FileMap.find(NormalizePath(pFilename));
    if (it == m_FileMap.end())
      return E_FAIL;

    CComPtr<IDxcBlobEncoding> pEncoding;
    IFR(m_pPdbUtils->GetSource(it->second, &pEncoding));

    return pEncoding.QueryInterface(ppIncludeSource);
  }
};

struct DxcPdbUtils : public IDxcPdbUtils, public IDxcPixDxilDebugInfoFactory
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()

  struct Source_File {
    std::wstring Name;
    CComPtr<IDxcBlobEncoding> Content;
  };

  CComPtr<IDxcBlob> m_InputBlob;
  CComPtr<IDxcBlob> m_pDebugProgramBlob;
  CComPtr<IDxcBlob> m_ContainerBlob;
  std::vector<Source_File> m_SourceFiles;
  std::vector<std::wstring> m_Defines;
  std::vector<std::wstring> m_Args;
  std::vector<std::wstring> m_Flags;
  std::wstring m_EntryPoint;
  std::wstring m_TargetProfile;
  std::wstring m_Name;
  std::wstring m_MainFileName;
  CComPtr<IDxcBlob> m_HashBlob;
  bool m_HasVersionInfo = false;
  hlsl::DxilCompilerVersion m_VersionInfo;

  void Reset() {
    m_pDebugProgramBlob = nullptr;
    m_InputBlob = nullptr;
    m_ContainerBlob = nullptr;
    m_SourceFiles.clear();
    m_Defines.clear();
    m_Args.clear();
    m_Flags.clear();
    m_EntryPoint.clear();
    m_TargetProfile.clear();
    m_Name.clear();
    m_MainFileName.clear();
    m_HashBlob = nullptr;
    m_HasVersionInfo = false;
    m_VersionInfo = {};
  }

  bool HasSources() const {
    return m_SourceFiles.size();
  }

  HRESULT PopulateSourcesFromProgramHeaderOrBitcode(IDxcBlob *pProgramBlob) {
    UINT32 bitcode_size = 0;
    const char *bitcode = nullptr;

    if (hlsl::IsValidDxilProgramHeader((hlsl::DxilProgramHeader *)pProgramBlob->GetBufferPointer(), pProgramBlob->GetBufferSize())) {
      hlsl::GetDxilProgramBitcode((hlsl::DxilProgramHeader *)pProgramBlob->GetBufferPointer(), &bitcode, &bitcode_size);
    }
    else if (IsBitcode(pProgramBlob->GetBufferPointer(), pProgramBlob->GetBufferSize())) {
      bitcode = (char *)pProgramBlob->GetBufferPointer();
      bitcode_size = pProgramBlob->GetBufferSize();
    }
    else {
      return E_INVALIDARG;
    }

    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> pModule;

    // NOTE: this doesn't copy the memory, just references it.
    std::unique_ptr<llvm::MemoryBuffer> mb = llvm::MemoryBuffer::getMemBuffer(StringRef(bitcode, bitcode_size), "-", /*RequiresNullTerminator*/ false);

    // Lazily parse the module
    std::string DiagStr;
    pModule = hlsl::dxilutil::LoadModuleFromBitcodeLazy(std::move(mb), context, DiagStr);
    if (!pModule)
      return E_FAIL;

    // Materialize only the stuff we need, so it's fast
    {
      llvm::StringRef DebugMetadataList[] = {
        hlsl::DxilMDHelper::kDxilSourceContentsMDName,
        hlsl::DxilMDHelper::kDxilSourceDefinesMDName,
        hlsl::DxilMDHelper::kDxilSourceArgsMDName,
        hlsl::DxilMDHelper::kDxilVersionMDName,
        hlsl::DxilMDHelper::kDxilShaderModelMDName,
        hlsl::DxilMDHelper::kDxilEntryPointsMDName,
        hlsl::DxilMDHelper::kDxilSourceMainFileNameMDName,
      };
      pModule->materializeSelectNamedMetadata(DebugMetadataList);
    }

    hlsl::DxilModule &DM = pModule->GetOrCreateDxilModule();
    m_EntryPoint = ToWstring(DM.GetEntryFunctionName());
    m_TargetProfile = ToWstring(DM.GetShaderModel()->GetName());

    // For each all the named metadata node in the module
    for (llvm::NamedMDNode &node : pModule->named_metadata()) {
      llvm::StringRef node_name = node.getName();

      // dx.source.content
      if (node_name == hlsl::DxilMDHelper::kDxilSourceContentsMDName ||
          node_name == hlsl::DxilMDHelper::kDxilSourceContentsOldMDName)
      {
        for (unsigned i = 0; i < node.getNumOperands(); i++) {
          llvm::MDTuple *tup = cast<llvm::MDTuple>(node.getOperand(i));
          MDString *md_name = cast<MDString>(tup->getOperand(0));
          MDString *md_content = cast<MDString>(tup->getOperand(1));

          // File name
          Source_File file;
          file.Name = ToWstring(md_name->getString());

          // File content
          IFR(hlsl::DxcCreateBlobWithEncodingOnHeapCopy(
            md_content->getString().data(),
            md_content->getString().size(),
            CP_ACP, // NOTE: ACP instead of UTF8 because it's possible for compiler implementations to
                    // inject non-UTF8 data here.
            &file.Content));

          m_SourceFiles.push_back(std::move(file));
        }
      }
      // dx.source.defines
      else if (node_name == hlsl::DxilMDHelper::kDxilSourceDefinesMDName ||
               node_name == hlsl::DxilMDHelper::kDxilSourceDefinesOldMDName)
      {
        MDTuple *tup = cast<MDTuple>(node.getOperand(0));
        for (unsigned i = 0; i < tup->getNumOperands(); i++) {
          StringRef define = cast<MDString>(tup->getOperand(i))->getString();
          m_Defines.push_back(ToWstring(define));
        }
      }
      // dx.source.mainFileName
      else if (node_name == hlsl::DxilMDHelper::kDxilSourceMainFileNameMDName ||
               node_name == hlsl::DxilMDHelper::kDxilSourceMainFileNameOldMDName)
      {
        MDTuple *tup = cast<MDTuple>(node.getOperand(0));
        MDString *str = cast<MDString>(tup->getOperand(0));
        m_MainFileName = ToWstring(str->getString());
      }
      // dx.source.args
      else if (node_name == hlsl::DxilMDHelper::kDxilSourceArgsMDName ||
               node_name == hlsl::DxilMDHelper::kDxilSourceArgsOldMDName)
      {
        MDTuple *tup = cast<MDTuple>(node.getOperand(0));
        // Args
        for (unsigned i = 0; i < tup->getNumOperands(); i++) {
          StringRef arg = cast<MDString>(tup->getOperand(i))->getString();
          m_Args.push_back(ToWstring(arg));
        }
        m_Flags = ComputeFlagsBasedOnArgs(m_Args);
      }
    }

    return S_OK;
  }

  HRESULT HandleDxilContainer(IDxcBlob *pContainer, IDxcBlob **ppDebugProgramBlob) {
    const hlsl::DxilContainerHeader *header = (hlsl::DxilContainerHeader *)m_ContainerBlob->GetBufferPointer();
    for (auto it = hlsl::begin(header); it != hlsl::end(header); it++) {
      const hlsl::DxilPartHeader *part = *it;
      hlsl::DxilFourCC four_cc = (hlsl::DxilFourCC)part->PartFourCC;

      switch (four_cc) {

      case hlsl::DFCC_CompilerVersion:
      {
        const hlsl::DxilCompilerVersion *header = (hlsl::DxilCompilerVersion *)(part+1);
        m_HasVersionInfo = true;
        memcpy(&m_VersionInfo, header, sizeof(m_VersionInfo));
      } break;

      case hlsl::DFCC_ShaderSourceInfo:
      {
        const hlsl::DxilSourceInfo *header = (hlsl::DxilSourceInfo *)(part+1);
        hlsl::SourceInfoReader reader;
        reader.Read(header);

        m_TargetProfile = ToWstring(reader.GetTargetProfile());
        m_EntryPoint = ToWstring(reader.GetEntryPoint());

        {
          StringRef definesData = reader.GetDefines();
          if (definesData.size()) {
            const char *def = definesData.data();
            while (true) {
              if (def[0] == 0)
                break;
              size_t def_len = strlen(def);
              m_Defines.push_back(ToWstring(def, def_len));
              def += def_len+1;
            }
          }
        }

        {
          StringRef argsData = reader.GetArgs();
          if (argsData.size()) {
            const char *arg = argsData.data();
            while (true) {
              if (arg[0] == 0)
                break;
              size_t arg_len = strlen(arg);
              m_Args.push_back(ToWstring(arg, arg_len));
              arg += arg_len+1;
            }
            m_Flags = ComputeFlagsBasedOnArgs(m_Args);
          }
        }

        for (unsigned i = 0; i < reader.GetSourcesCount(); i++) {
          hlsl::SourceInfoReader::Source source_data = reader.GetSource(i);

          Source_File source;
          source.Name = ToWstring(source_data.Name);
          IFR(hlsl::DxcCreateBlobWithEncodingOnHeapCopy(
            source_data.Content.data(),
            source_data.Content.size(),
            CP_ACP, // NOTE: ACP instead of UTF8 because it's possible for compiler implementations to
                    // inject non-UTF8 data here.
            &source.Content));

          // First file is the main file
          if (i == 0) {
            m_MainFileName = source.Name;
          }

          m_SourceFiles.push_back(std::move(source));
        }

      } break;

      case hlsl::DFCC_ShaderHash:
      {
        const hlsl::DxilShaderHash *hash_header = (hlsl::DxilShaderHash *)(part+1);
        IFR(hlsl::DxcCreateBlobOnHeapCopy(hash_header, sizeof(*hash_header), &m_HashBlob));
      } break;

      case hlsl::DFCC_ShaderDebugName:
      {
        const hlsl::DxilShaderDebugName *name_header = (hlsl::DxilShaderDebugName *)(part+1);
        const char *ptr = (char *)(name_header+1);
        m_Name = ToWstring(ptr, name_header->NameLength);
      } break;

      case hlsl::DFCC_ShaderDebugInfoDXIL:
      {
        hlsl::DxilProgramHeader *program_header = (hlsl::DxilProgramHeader *)(part+1);

        CComPtr<IDxcBlobEncoding> pProgramHeaderBlob;
        IFR(hlsl::DxcCreateBlobWithEncodingFromPinned(program_header, program_header->SizeInUint32*sizeof(UINT32), CP_ACP, &pProgramHeaderBlob));
        IFR(pProgramHeaderBlob.QueryInterface(ppDebugProgramBlob));

      } break; // hlsl::DFCC_ShaderDebugInfoDXIL
      } // switch (four_cc)
    } // For each part

    return S_OK;
  }

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(DxcPdbUtils)

  DxcPdbUtils(IMalloc *pMalloc) : m_dwRef(0), m_pMalloc(pMalloc) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcPdbUtils, IDxcPixDxilDebugInfoFactory>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pPdbOrDxil) override {
    DxcThreadMalloc TM(m_pMalloc);

    ::llvm::sys::fs::MSFileSystem *msfPtr = nullptr;
    IFT(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
  
    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    if (!pPdbOrDxil)
      return E_POINTER;

    // Remove all the data
    Reset();

    m_InputBlob = pPdbOrDxil;

    try {
      CComPtr<IStream> pStream;
      IFR(hlsl::CreateReadOnlyBlobStream(pPdbOrDxil, &pStream));

      // PDB
      if (SUCCEEDED(hlsl::pdb::LoadDataFromStream(m_pMalloc, pStream, &m_ContainerBlob))) {
        IFR(HandleDxilContainer(m_ContainerBlob, &m_pDebugProgramBlob));
        if (!HasSources()) {
          if (m_pDebugProgramBlob) {
            IFR(PopulateSourcesFromProgramHeaderOrBitcode(m_pDebugProgramBlob));
          }
          else {
            return E_FAIL;
          }
        }
      }
      // DXIL Container
      else if (hlsl::IsValidDxilContainer((const hlsl::DxilContainerHeader *)pPdbOrDxil->GetBufferPointer(), pPdbOrDxil->GetBufferSize())) {
        m_ContainerBlob = pPdbOrDxil;
        IFR(HandleDxilContainer(m_ContainerBlob, &m_pDebugProgramBlob));
        // If we have a Debug DXIL, populate the debug info.
        if (m_pDebugProgramBlob && !HasSources()) {
          IFR(PopulateSourcesFromProgramHeaderOrBitcode(m_pDebugProgramBlob));
        }
      }
      // DXIL program header or bitcode
      else {
        CComPtr<IDxcBlobEncoding> pProgramHeaderBlob;
        IFR(hlsl::DxcCreateBlobWithEncodingFromPinned(
          (hlsl::DxilProgramHeader *)pPdbOrDxil->GetBufferPointer(),
          pPdbOrDxil->GetBufferSize(), CP_ACP, &pProgramHeaderBlob));
        IFR(pProgramHeaderBlob.QueryInterface(&m_pDebugProgramBlob));
        IFR(PopulateSourcesFromProgramHeaderOrBitcode(m_pDebugProgramBlob));
      }
    }
    catch (std::bad_alloc) {
      Reset();
      return E_OUTOFMEMORY;
    }

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetSourceCount(_Out_ UINT32 *pCount) override {
    if (!pCount) return E_POINTER;
    *pCount = (UINT32)m_SourceFiles.size();
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetSource(_In_ UINT32 uIndex, _COM_Outptr_ IDxcBlobEncoding **ppResult) override {
    if (uIndex >= m_SourceFiles.size()) return E_INVALIDARG;
    if (!ppResult) return E_POINTER;
    *ppResult = nullptr;
    return m_SourceFiles[uIndex].Content.QueryInterface(ppResult);
  }

  virtual HRESULT STDMETHODCALLTYPE GetSourceName(_In_ UINT32 uIndex, _Outptr_result_z_ BSTR *pResult) override {
    if (uIndex >= m_SourceFiles.size()) return E_INVALIDARG;
    return CopyWstringToBSTR(m_SourceFiles[uIndex].Name, pResult);
  }

  static inline HRESULT GetStringCount(const std::vector<std::wstring> &list, _Out_ UINT32 *pCount) {
    if (!pCount) return E_POINTER;
    *pCount = (UINT32)list.size();
    return S_OK;
  }

  static inline HRESULT GetStringOption(const std::vector<std::wstring> &list, _In_ UINT32 uIndex, _Outptr_result_z_ BSTR *pResult) {
    if (uIndex >= list.size()) return E_INVALIDARG;
    return CopyWstringToBSTR(list[uIndex], pResult);
  }

  virtual HRESULT STDMETHODCALLTYPE GetFlagCount(_Out_ UINT32 *pCount) override {  return GetStringCount(m_Flags, pCount); }
  virtual HRESULT STDMETHODCALLTYPE GetFlag(_In_ UINT32 uIndex, _Outptr_result_z_ BSTR *pResult) override { return GetStringOption(m_Flags, uIndex, pResult); }
  virtual HRESULT STDMETHODCALLTYPE GetArgCount(_Out_ UINT32 *pCount) override { return GetStringCount(m_Args, pCount); }
  virtual HRESULT STDMETHODCALLTYPE GetArg(_In_ UINT32 uIndex, _Outptr_result_z_ BSTR *pResult) override { return GetStringOption(m_Args, uIndex, pResult); }
  virtual HRESULT STDMETHODCALLTYPE GetDefineCount(_Out_ UINT32 *pCount) override { return GetStringCount(m_Defines, pCount); }
  virtual HRESULT STDMETHODCALLTYPE GetDefine(_In_ UINT32 uIndex, _Outptr_result_z_ BSTR *pResult) override { return GetStringOption(m_Defines, uIndex, pResult); }

  virtual HRESULT STDMETHODCALLTYPE GetTargetProfile(_Outptr_result_z_ BSTR *pResult) override {
    return CopyWstringToBSTR(m_TargetProfile, pResult);
  }
  virtual HRESULT STDMETHODCALLTYPE GetEntryPoint(_Outptr_result_z_ BSTR *pResult) override {
    return CopyWstringToBSTR(m_EntryPoint, pResult);
  }
  virtual HRESULT STDMETHODCALLTYPE GetMainFileName(_Outptr_result_z_ BSTR *pResult) {
    return CopyWstringToBSTR(m_MainFileName, pResult);
  }

  virtual BOOL STDMETHODCALLTYPE IsFullPDB() override {
    return m_pDebugProgramBlob != nullptr;
  }

  virtual HRESULT STDMETHODCALLTYPE GetFullPDB(_COM_Outptr_ IDxcBlob **ppFullPDB) override {
    if (!m_InputBlob)
      return E_FAIL;

    if (!ppFullPDB) return E_POINTER;

    *ppFullPDB = nullptr;

    // If we are already a full pdb, just return the input blob
    if (IsFullPDB()) {
      return m_InputBlob.QueryInterface(ppFullPDB);
    }

    CComPtr<IDxcCompiler3> pCompiler;
    IFR(DxcCreateInstance2(m_pMalloc, CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void **)&pCompiler));

    DxcThreadMalloc TM(m_pMalloc);

    std::vector<const WCHAR *> new_args;
    for (unsigned i = 0; i < m_Args.size(); i++) {
      if (m_Args[i] == L"/Qslim_debug" || m_Args[i] == L"-Qslim_debug")
        continue;
      new_args.push_back(m_Args[i].c_str());
    }
    new_args.push_back(L"-Qfull_debug");

    assert(m_MainFileName.size());
    if (m_MainFileName.size())
      new_args.push_back(m_MainFileName.c_str());

    if (m_SourceFiles.empty())
      return E_FAIL;

    CComPtr<PdbRecompilerIncludeHandler> pIncludeHandler = CreateOnMalloc<PdbRecompilerIncludeHandler>(m_pMalloc);
    if (!pIncludeHandler)
      return E_OUTOFMEMORY;

    pIncludeHandler->m_pPdbUtils = this;
    for (unsigned i = 0; i < m_SourceFiles.size(); i++) {
      std::wstring NormalizedName = NormalizePath(m_SourceFiles[i].Name.c_str());
      pIncludeHandler->m_FileMap.insert(std::pair<std::wstring, unsigned>(NormalizedName, i));
    }

    IDxcBlobEncoding *main_file = m_SourceFiles[0].Content;

    DxcBuffer source_buf = {};
    source_buf.Ptr = main_file->GetBufferPointer();
    source_buf.Size = main_file->GetBufferSize();
    BOOL bEndodingKnown = FALSE;
    IFR(main_file->GetEncoding(&bEndodingKnown, &source_buf.Encoding));

    CComPtr<IDxcResult> pResult;
    IFR(pCompiler->Compile(&source_buf, new_args.data(), new_args.size(), pIncludeHandler, __uuidof(IDxcResult), (void **)&pResult));

    CComPtr<IDxcOperationResult> pOperationResult;
    IFR(pResult.QueryInterface(&pOperationResult));

    HRESULT CompileResult = S_OK;
    IFR(pOperationResult->GetStatus(&CompileResult));
    IFR(CompileResult);

    CComPtr<IDxcBlob> pFullPDB;
    CComPtr<IDxcBlobUtf16> pFullPDBName;
    IFR(pResult->GetOutput(DXC_OUT_PDB, __uuidof(IDxcBlob), (void **)&pFullPDB, &pFullPDBName));

    return pFullPDB.QueryInterface(ppFullPDB);
  }

  virtual HRESULT STDMETHODCALLTYPE GetHash(_COM_Outptr_ IDxcBlob **ppResult) override {
    if (!ppResult) return E_POINTER;
    *ppResult = nullptr;
    if (m_HashBlob)
      return m_HashBlob.QueryInterface(ppResult);
    return E_FAIL;
  }

  virtual HRESULT STDMETHODCALLTYPE GetName(_Outptr_result_z_ BSTR *pResult) override {
    return CopyWstringToBSTR(m_Name, pResult);
  }

  virtual STDMETHODIMP NewDxcPixDxilDebugInfo(
      _COM_Outptr_ IDxcPixDxilDebugInfo **ppDxilDebugInfo) override
  {
    if (!m_pDebugProgramBlob)
      return E_FAIL;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDiaDataSource> pDataSource;
    IFR(DxcCreateInstance2(m_pMalloc, CLSID_DxcDiaDataSource, __uuidof(IDiaDataSource),
      (void **)&pDataSource));

    CComPtr<IStream> pStream;
    IFR(hlsl::CreateReadOnlyBlobStream(m_pDebugProgramBlob, &pStream));

    IFR(pDataSource->loadDataFromIStream(pStream));

    CComPtr<IDiaSession> pSession;
    IFR(pDataSource->openSession(&pSession));

    CComPtr<IDxcPixDxilDebugInfoFactory> pFactory;
    IFR(pSession.QueryInterface(&pFactory));

    return pFactory->NewDxcPixDxilDebugInfo(ppDxilDebugInfo);
  }

  virtual STDMETHODIMP NewDxcPixCompilationInfo(
      _COM_Outptr_ IDxcPixCompilationInfo **ppCompilationInfo) override
  {
    return E_NOTIMPL;
  }

  virtual HRESULT STDMETHODCALLTYPE GetVersionInfo(_COM_Outptr_ IDxcVersionInfo **ppVersionInfo) {
    if (!ppVersionInfo)
      return E_POINTER;

    *ppVersionInfo = nullptr;
    if (!m_HasVersionInfo)
      return E_FAIL;

    CComPtr<DxcPdbVersionInfo> result = CreateOnMalloc<DxcPdbVersionInfo>(m_pMalloc);
    if (result == nullptr) {
      return E_OUTOFMEMORY;
    }
    result->m_Version = m_VersionInfo;
    *ppVersionInfo = result.Detach();
    return S_OK;
  }
};

HRESULT CreateDxcPdbUtils(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  CComPtr<DxcPdbUtils> result = CreateOnMalloc<DxcPdbUtils>(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }
  return result.p->QueryInterface(riid, ppv);
}

#else

#include "dxc/Support/WinIncludes.h"

HRESULT CreateDxcPdbUtils(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  return E_NOTIMPL;
}

#endif


