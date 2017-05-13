///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcompilerobj.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Compiler.                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/HLSLMacroExpander.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Support/Host.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "llvm/IR/LLVMContext.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/HLSLExtensionsCodegenHelper.h"
#include "dxc/HLSL/DxilRootSignature.h"
#include "dxcutil.h"

// SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
#include "clang/SPIRV/EmitSPIRVAction.h"
#endif
// SPIRV change ends

#if defined(_MSC_VER)
#include <io.h>
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif
#endif

#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilResource.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilOperations.h"

#include "dxc/dxcapi.internal.h"

#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/DxcLangExtensionsHelper.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxcetw.h"
#include "dxillib.h"
#include <algorithm>

#define CP_UTF16 1200

#ifdef DBG

// This should be improved with global enabled mask rather than a compile-time mask.
#define DXTRACE_MASK_ENABLED  0
#define DXTRACE_MASK_APIFS    1
#define DXTRACE_ENABLED(subsystem) (DXTRACE_MASK_ENABLED & subsystem)

// DXTRACE_FMT formats a debugger trace message if DXTRACE_MASK allows it.
#define DXTRACE_FMT(subsystem, fmt, ...) do { \
  if (DXTRACE_ENABLED(subsystem)) OutputDebugFormatA(fmt, __VA_ARGS__); \
} while (0)
/// DXTRACE_FMT_APIFS is used by the API-based virtual filesystem.
#define DXTRACE_FMT_APIFS(fmt, ...) DXTRACE_FMT(DXTRACE_MASK_APIFS, fmt, __VA_ARGS__)

#else

#define DXTRACE_FMT_APIFS(...)

#endif // DBG

using namespace llvm;
using namespace clang;
using namespace hlsl;
using std::string;

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);

// This internal call allows the validator to avoid having to re-deserialize
// the module. It trusts that the caller didn't make any changes and is
// kept internal because the layout of the module class may change based
// on changes across modules, or picking a different compiler version or CRT.
HRESULT RunInternalValidator(_In_ IDxcValidator *pValidator,
                             _In_ llvm::Module *pModule,
                             _In_ llvm::Module *pDebugModule,
                             _In_ IDxcBlob *pShader, UINT32 Flags,
                             _In_ IDxcOperationResult **ppResult);

enum class HandleKind {
  Special = 0,
  File = 1,
  FileDir = 2,
  SearchDir = 3
};
enum class SpecialValue {
  Unknown = 0,
  StdOut = 1,
  StdErr = 2,
  Source = 3,
  Output = 4
};
struct HandleBits {
  unsigned Offset : 8;
  unsigned Length : 8;
  unsigned Kind : 4;
};
struct DxcArgsHandle {
  DxcArgsHandle(HANDLE h) : Handle(h) {}
  DxcArgsHandle(unsigned fileIndex)
    : Bits{ fileIndex, 0, (unsigned)HandleKind::File } {}
  DxcArgsHandle(HandleKind HK, unsigned fileIndex, unsigned dirLength)
    : Bits{ fileIndex, dirLength, (unsigned)HK} {}
  DxcArgsHandle(SpecialValue V)
      : Bits{(unsigned)V, 0, (unsigned)HandleKind::Special} {}
  union {
    HANDLE Handle;
    HandleBits Bits;
  };
  bool operator==(const DxcArgsHandle &Other) { return Handle == Other.Handle; }
  HandleKind GetKind() const { return (HandleKind)Bits.Kind; }
  bool IsFileKind() const { return GetKind() == HandleKind::File; }
  bool IsSpecialUnknown() const { return Handle == 0; }
  bool IsDirHandle() const {
    return GetKind() == HandleKind::FileDir || GetKind() == HandleKind::SearchDir;
  }
  bool IsStdHandle() const {
    return GetKind() == HandleKind::Special &&
           (GetSpecialValue() == SpecialValue::StdErr ||
            GetSpecialValue() == SpecialValue::StdOut);
  }
  unsigned GetFileIndex() const {
    DXASSERT_NOMSG(IsFileKind());
    return Bits.Offset;
  }
  SpecialValue GetSpecialValue() const {
    DXASSERT_NOMSG(GetKind() == HandleKind::Special);
    return (SpecialValue)Bits.Offset;
  }
  unsigned Length() const { return Bits.Length; }
};

static_assert(sizeof(DxcArgsHandle) == sizeof(HANDLE), "else can't transparently typecast");

static const DxcArgsHandle UnknownHandle(SpecialValue::Unknown);
static const DxcArgsHandle StdOutHandle(SpecialValue::StdOut);
static const DxcArgsHandle StdErrHandle(SpecialValue::StdErr);
static const DxcArgsHandle OutputHandle(SpecialValue::Output);

/// Max number of included files (1:1 to their directories) or search directories.
/// If programs include more than a handful, DxcArgsFileSystem will need to do better than linear scans.
/// If this is fired, ERROR_OUT_OF_STRUCTURES will be returned by an attempt to open a file.
static const size_t MaxIncludedFiles = 200;

static bool IsAbsoluteOrCurDirRelativeW(LPCWSTR Path) {
  if (!Path || !Path[0]) return FALSE;
  // Current dir-relative path.
  if (Path[0] == L'.') {
    return Path[1] == L'\0' || Path[1] == L'/' || Path[1] == L'\\';
  }
  // Disk designator, then absolute path.
  if (Path[1] == L':' && Path[2] == L'\\') {
    return TRUE;
  }
  // UNC name
  if (Path[0] == L'\\') {
    return Path[1] == L'\\';
  }

  //
  // NOTE: there are a number of cases we don't handle, as they don't play well with the simple
  // file system abstraction we use:
  // - current directory on disk designator (eg, D:file.ext), requires per-disk current dir
  // - parent paths relative to current directory (eg, ..\\file.ext)
  //
  // The current-directory support is available to help in-memory handlers. On-disk handlers
  // will typically have absolute paths to begin with.
  //
  return FALSE;
}

static void MakeAbsoluteOrCurDirRelativeW(LPCWSTR &Path, std::wstring &PathStorage) {
  if (IsAbsoluteOrCurDirRelativeW(Path)) {
    return;
  }
  else {
    PathStorage = L"./";
    PathStorage += Path;
    Path = PathStorage.c_str();
  }
}

static bool IsAbsoluteOrCurDirRelative(const Twine &T) {
  if (llvm::sys::path::is_absolute(T)) {
    return true;
  }
  if (T.isSingleStringRef()) {
    StringRef r = T.getSingleStringRef();
    if (r.size() < 2) return false;
    const char *pData = r.data();
    return pData[0] == '.' && (pData[1] == '\\' || pData[1] == '/');
  }
  DXASSERT(false, "twine kind not supported");
  return false;
}

/// File system based on API arguments. Support being added incrementally.
///
/// DxcArgsFileSystem emulates a file system to clang/llvm based on API
/// arguments. It can block certain functionality (like picking up the current
/// directory), while adding other (like supporting an app's in-memory
/// files through an IDxcIncludeHandler).
///
/// stdin/stdout/stderr are registered especially (given that they have a
/// special role in llvm::ins/outs/errs and are defaults to various operations,
/// it's not unexpected). The direct user of DxcArgsFileSystem can also register
/// streams to capture output for specific files.
///
/// Support for IDxcIncludeHandler is somewhat tricky because the API is very
/// minimal, to allow simple implementations, but that puts this class in the
/// position of brokering between llvm/clang existing files (which probe for
/// files and directories in various patterns), and this simpler handler.
/// The current approach is to minimize changes in llvm/clang and work around
/// the absence of directory support in IDxcIncludeHandler by assuming all
/// included paths already exist (the handler may reject those paths later on),
/// and always querying for a file before its parent directory (so we can
/// disambiguate between one or the other).
class DxcArgsFileSystem : public ::llvm::sys::fs::MSFileSystem {
private:
  CComPtr<IDxcBlob> m_pSource;
  LPCWSTR m_pSourceName;
  std::wstring m_pAbsSourceName; // absolute (or '.'-relative) source name
  CComPtr<IStream> m_pSourceStream;
  CComPtr<IStream> m_pOutputStream;
  CComPtr<AbstractMemoryStream> m_pStdOutStream;
  CComPtr<AbstractMemoryStream> m_pStdErrStream;
  LPCWSTR m_pOutputStreamName;
  std::wstring m_pAbsOutputStreamName;
  CComPtr<IDxcIncludeHandler> m_includeLoader;
  std::vector<std::wstring> m_searchEntries;
  bool m_bDisplayIncludeProcess;

  // Some constraints of the current design: opening the same file twice
  // will return the same handle/structure, and thus the same file pointer.
  struct IncludedFile {
    CComPtr<IDxcBlob> Blob;
    CComPtr<IStream> BlobStream;
    std::wstring Name;
    IncludedFile(std::wstring &&name, IDxcBlob *pBlob, IStream *pStream)
      : Name(name), Blob(pBlob), BlobStream(pStream) { }
  };
  llvm::SmallVector<IncludedFile, 4> m_includedFiles;

  static bool IsDirOf(LPCWSTR lpDir, size_t dirLen, const std::wstring &fileName) {
    if (fileName.size() <= dirLen) return false;
    if (0 != wcsncmp(lpDir, fileName.data(), dirLen)) return false;

    // Prefix matches, c:\\ to c:\\foo.hlsl or ./bar to ./bar/file.hlsl
    // Ensure there are no additional characters, don't match ./ba if ./bar.hlsl exists
    if (lpDir[dirLen - 1] == '\\' || lpDir[dirLen - 1] == '/') {
      // The file name was already terminated in a separator.
      return true;
    }

    return fileName.data()[dirLen] == '\\' || fileName.data()[dirLen] == '/';
  }

  static bool IsDirPrefixOrSame(LPCWSTR lpDir, size_t dirLen, const std::wstring &path) {
    if (0 == wcscmp(lpDir, path.c_str())) return true;
    return IsDirOf(lpDir, dirLen, path);
  }

  HANDLE TryFindDirHandle(LPCWSTR lpDir) const {
    size_t dirLen = wcslen(lpDir);
    for (size_t i = 0; i < m_includedFiles.size(); ++i) {
      const std::wstring &fileName = m_includedFiles[i].Name;
      if (IsDirOf(lpDir, dirLen, fileName)) {
        return DxcArgsHandle(HandleKind::FileDir, i, dirLen).Handle;
      }
    }
    for (size_t i = 0; i < m_searchEntries.size(); ++i) {
      if (IsDirPrefixOrSame(lpDir, dirLen, m_searchEntries[i])) {
        return DxcArgsHandle(HandleKind::SearchDir, i, dirLen).Handle;
      }
    }
    return INVALID_HANDLE_VALUE;
  }
  DWORD TryFindOrOpen(LPCWSTR lpFileName, size_t &index) {
    for (size_t i = 0; i < m_includedFiles.size(); ++i) {
      if (0 == wcscmp(lpFileName, m_includedFiles[i].Name.data())) {
        index = i;
        return ERROR_SUCCESS;
      }
    }

    if (m_includeLoader.p != nullptr) {
      if (m_includedFiles.size() == MaxIncludedFiles) {
        return ERROR_OUT_OF_STRUCTURES;
      }

      CComPtr<IDxcBlob> fileBlob;
      HRESULT hr = m_includeLoader->LoadSource(lpFileName, &fileBlob);
      if (FAILED(hr)) {
        return ERROR_UNHANDLED_EXCEPTION;
      }
      if (fileBlob.p != nullptr) {
        CComPtr<IDxcBlobEncoding> fileBlobEncoded;
        if (FAILED(hlsl::DxcGetBlobAsUtf8(fileBlob, &fileBlobEncoded))) {
          return ERROR_UNHANDLED_EXCEPTION;
        }
        CComPtr<IStream> fileStream;
        if (FAILED(hlsl::CreateReadOnlyBlobStream(fileBlobEncoded, &fileStream))) {
          return ERROR_UNHANDLED_EXCEPTION;
        }
        m_includedFiles.emplace_back(std::wstring(lpFileName), fileBlobEncoded, fileStream);
        index = m_includedFiles.size() - 1;

        if (m_bDisplayIncludeProcess) {
          std::string openFileStr;
          raw_string_ostream s(openFileStr);
          std::string fileName = Unicode::UTF16ToUTF8StringOrThrow(lpFileName);
          s << "Opening file [" << fileName << "], stack top [" << (index-1)
            << "]\n";
          s.flush();
          ULONG cbWritten;
          IFT(m_pStdErrStream->Write(openFileStr.c_str(), openFileStr.size(),
                                 &cbWritten));
        }
        return ERROR_SUCCESS;
      }
    }
    return ERROR_NOT_FOUND;
  }
  static HANDLE IncludedFileIndexToHandle(size_t index) {
    return DxcArgsHandle(index).Handle;
  }
  bool IsKnownHandle(HANDLE h) const {
    return !DxcArgsHandle(h).IsSpecialUnknown();
  }
  IncludedFile &HandleToIncludedFile(HANDLE handle) {
    DxcArgsHandle argsHandle(handle);
    DXASSERT_NOMSG(argsHandle.GetFileIndex() < m_includedFiles.size());
    return m_includedFiles[argsHandle.GetFileIndex()];
  }

public:
  DxcArgsFileSystem(_In_ IDxcBlob *pSource, LPCWSTR pSourceName, _In_opt_ IDxcIncludeHandler* pHandler)
      : m_pSource(pSource), m_pSourceName(pSourceName), m_includeLoader(pHandler), m_bDisplayIncludeProcess(false),
        m_pOutputStreamName(nullptr) {
    MakeAbsoluteOrCurDirRelativeW(m_pSourceName, m_pAbsSourceName);
    IFT(CreateReadOnlyBlobStream(m_pSource, &m_pSourceStream));
    m_includedFiles.push_back(IncludedFile(std::wstring(m_pSourceName), m_pSource, m_pSourceStream));
  }
  void EnableDisplayIncludeProcess() {
    m_bDisplayIncludeProcess = true;
  }
  void WriteStdErrToStream(raw_string_ostream &s) {
    s.write((char*)m_pStdErrStream->GetPtr(), m_pStdErrStream->GetPtrSize());
    s.flush();
  }
  HRESULT CreateStdStreams(_In_ IMalloc* pMalloc) {
    DXASSERT(m_pStdOutStream == nullptr, "else already created");
    CreateMemoryStream(pMalloc, &m_pStdOutStream);
    CreateMemoryStream(pMalloc, &m_pStdErrStream);
    if (m_pStdOutStream == nullptr || m_pStdErrStream == nullptr) {
      return E_OUTOFMEMORY;
    }
    return S_OK;
  }

  void GetStreamForFD(int fd, IStream** ppResult) {
    return GetStreamForHandle(HandleFromFD(fd), ppResult);
  }
  void GetStreamForHandle(HANDLE handle, IStream** ppResult) {
    CComPtr<IStream> stream;
    DxcArgsHandle argsHandle(handle);
    if (argsHandle == OutputHandle) {
      stream = m_pOutputStream;
    }
    else if (argsHandle == StdOutHandle) {
      stream = m_pStdOutStream;
    }
    else if (argsHandle == StdErrHandle) {
      stream = m_pStdErrStream;
    }
    else if (argsHandle.GetKind() == HandleKind::File) {
      stream = HandleToIncludedFile(handle).BlobStream;
    }
    *ppResult = stream.Detach();
  }

  void SetupForCompilerInstance(CompilerInstance &compiler) {
    DXASSERT(m_searchEntries.size() == 0, "else compiler instance being set twice");
    // Turn these into UTF-16 to avoid converting later, and ensure they
    // are fully-qualified or relative to the current directory.
    const std::vector<HeaderSearchOptions::Entry> &entries =
      compiler.getHeaderSearchOpts().UserEntries;
    if (entries.size() > MaxIncludedFiles) {
      throw hlsl::Exception(HRESULT_FROM_WIN32(ERROR_OUT_OF_STRUCTURES));
    }
    for (unsigned i = 0, e = entries.size(); i != e; ++i) {
      const HeaderSearchOptions::Entry &E = entries[i];
      if (IsAbsoluteOrCurDirRelative(E.Path)) {
        m_searchEntries.emplace_back(Unicode::UTF8ToUTF16StringOrThrow(E.Path.c_str()));
      }
      else {
        std::wstring ws(L"./");
        ws += Unicode::UTF8ToUTF16StringOrThrow(E.Path.c_str());
        m_searchEntries.emplace_back(std::move(ws));
      }
    }
  }

  HRESULT RegisterOutputStream(LPCWSTR pName, IStream *pStream) {
    DXASSERT(m_pOutputStream.p == nullptr, "else multiple outputs registered");
    m_pOutputStream = pStream;
    m_pOutputStreamName = pName;
    MakeAbsoluteOrCurDirRelativeW(m_pOutputStreamName, m_pAbsOutputStreamName);
    return S_OK;
  }

  __override ~DxcArgsFileSystem() { };
  __override BOOL FindNextFileW(
    _In_   HANDLE hFindFile,
    _Out_  LPWIN32_FIND_DATAW lpFindFileData) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }

  __override HANDLE FindFirstFileW(
    _In_   LPCWSTR lpFileName,
    _Out_  LPWIN32_FIND_DATAW lpFindFileData) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }

  __override void FindClose(HANDLE findHandle) throw() {
    __debugbreak();
  }

  __override HANDLE CreateFileW(
    _In_      LPCWSTR lpFileName,
    _In_      DWORD dwDesiredAccess,
    _In_      DWORD dwShareMode,
    _In_      DWORD dwCreationDisposition,
    _In_      DWORD dwFlagsAndAttributes) throw() {
    DXTRACE_FMT_APIFS("DxcArgsFileSystem::CreateFileW %S\n", lpFileName);
    size_t sourceNameLen = wcslen(m_pSourceName);
    std::wstring FileNameStore;
    MakeAbsoluteOrCurDirRelativeW(lpFileName, FileNameStore);
    size_t fileNameLen = wcslen(lpFileName);

    // Check for a match to the output file.
    if (m_pOutputStreamName != nullptr &&
        0 == wcscmp(lpFileName, m_pOutputStreamName)) {
      return OutputHandle.Handle;
    }

    HANDLE dirHandle = TryFindDirHandle(lpFileName);
    if (dirHandle != INVALID_HANDLE_VALUE) {
      return dirHandle;
    }

    size_t includedIndex;
    DWORD findError = TryFindOrOpen(lpFileName, includedIndex);
    if (findError == ERROR_SUCCESS) {
      return IncludedFileIndexToHandle(includedIndex);
    }

    SetLastError(findError);
    return INVALID_HANDLE_VALUE;
  }

  __override BOOL SetFileTime(_In_ HANDLE hFile,
    _In_opt_  const FILETIME *lpCreationTime,
    _In_opt_  const FILETIME *lpLastAccessTime,
    _In_opt_  const FILETIME *lpLastWriteTime) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }

  __override BOOL GetFileInformationByHandle(_In_ HANDLE hFile, _Out_ LPBY_HANDLE_FILE_INFORMATION lpFileInformation) throw() {
    DxcArgsHandle argsHandle(hFile);
    ZeroMemory(lpFileInformation, sizeof(*lpFileInformation));
    lpFileInformation->nFileIndexLow = (DWORD)(uintptr_t)hFile;
    if (argsHandle.IsFileKind()) {
      IncludedFile &file = HandleToIncludedFile(hFile);
      lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
      lpFileInformation->nFileSizeLow = file.Blob->GetBufferSize();
      return TRUE;
    }
    if (argsHandle == OutputHandle) {
      lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
      STATSTG stat;
      HRESULT hr = m_pOutputStream->Stat(&stat, STATFLAG_NONAME);
      if (FAILED(hr)) {
        SetLastError(ERROR_IO_DEVICE);
        return FALSE;
      }
      lpFileInformation->nFileSizeLow = stat.cbSize.LowPart;
      return TRUE;
    }
    else if (argsHandle.IsDirHandle()) {
      lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
      lpFileInformation->nFileIndexHigh = 1;
      return TRUE;
    }

    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  __override DWORD GetFileType(_In_ HANDLE hFile) throw() {
    DxcArgsHandle argsHandle(hFile);
    if (argsHandle.IsStdHandle()) {
      return FILE_TYPE_CHAR;
    }
    // Every other known handle is of type disk.
    if (!argsHandle.IsSpecialUnknown()) {
      return FILE_TYPE_DISK;
    }

    SetLastError(ERROR_NOT_FOUND);
    return FILE_TYPE_UNKNOWN;
  }

  __override BOOL CreateHardLinkW(_In_ LPCWSTR lpFileName, _In_ LPCWSTR lpExistingFileName) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override BOOL MoveFileExW(_In_ LPCWSTR lpExistingFileName, _In_opt_ LPCWSTR lpNewFileName, _In_ DWORD dwFlags) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override DWORD GetFileAttributesW(_In_ LPCWSTR lpFileName) throw() {
    DXTRACE_FMT_APIFS("DxcArgsFileSystem::GetFileAttributesW %S\n", lpFileName);
    std::wstring FileNameStore;
    MakeAbsoluteOrCurDirRelativeW(lpFileName, FileNameStore);
    size_t sourceNameLen = wcslen(m_pSourceName);
    size_t fileNameLen = wcslen(lpFileName);

    // Check for a match to the source.
    if (fileNameLen == sourceNameLen) {
      if (0 == wcsncmp(m_pSourceName, lpFileName, fileNameLen)) {
        return FILE_ATTRIBUTE_NORMAL;
      }
    }

    // Check for a perfect match to the output.
    if (m_pOutputStreamName != nullptr &&
        0 == wcscmp(m_pOutputStreamName, lpFileName)) {
      return FILE_ATTRIBUTE_NORMAL;
    }

    if (TryFindDirHandle(lpFileName) != INVALID_HANDLE_VALUE) {
      return FILE_ATTRIBUTE_DIRECTORY;
    }

    size_t includedIndex;
    DWORD findError = TryFindOrOpen(lpFileName, includedIndex);
    if (findError == ERROR_SUCCESS) {
      return FILE_ATTRIBUTE_NORMAL;
    }

    SetLastError(findError);
    return INVALID_FILE_ATTRIBUTES;
  }

  __override BOOL CloseHandle(_In_ HANDLE hObject) throw() {
    // Not actually closing handle. Would allow improper usage, but simplifies
    // query/open/usage patterns.
    if (IsKnownHandle(hObject)) {
      return TRUE;
    }

    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  __override BOOL DeleteFileW(_In_ LPCWSTR lpFileName) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override BOOL RemoveDirectoryW(_In_ LPCWSTR lpFileName) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override BOOL CreateDirectoryW(_In_ LPCWSTR lpPathName) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  _Success_(return != 0 && return < nBufferLength)
    __override DWORD GetCurrentDirectoryW(_In_ DWORD nBufferLength, _Out_writes_to_opt_(nBufferLength, return +1) LPWSTR lpBuffer) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  _Success_(return != 0 && return < nSize)
    __override DWORD GetMainModuleFileNameW(__out_ecount_part(nSize, return +1) LPWSTR lpFilename, DWORD nSize) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override DWORD GetTempPathW(DWORD nBufferLength, _Out_writes_to_opt_(nBufferLength, return +1) LPWSTR lpBuffer) {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override BOOLEAN CreateSymbolicLinkW(_In_ LPCWSTR lpSymlinkFileName, _In_ LPCWSTR lpTargetFileName, DWORD dwFlags) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override bool SupportsCreateSymbolicLink() throw() {
    return false;
  }
  __override BOOL ReadFile(_In_ HANDLE hFile, _Out_bytecap_(nNumberOfBytesToRead) LPVOID lpBuffer, _In_ DWORD nNumberOfBytesToRead, _Out_opt_ LPDWORD lpNumberOfBytesRead) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }
  __override HANDLE CreateFileMappingW(
    _In_      HANDLE hFile,
    _In_      DWORD flProtect,
    _In_      DWORD dwMaximumSizeHigh,
    _In_      DWORD dwMaximumSizeLow) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return INVALID_HANDLE_VALUE;
  }
  __override LPVOID MapViewOfFile(
    _In_  HANDLE hFileMappingObject,
    _In_  DWORD dwDesiredAccess,
    _In_  DWORD dwFileOffsetHigh,
    _In_  DWORD dwFileOffsetLow,
    _In_  SIZE_T dwNumberOfBytesToMap) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return nullptr;
  }
  __override BOOL UnmapViewOfFile(_In_ LPCVOID lpBaseAddress) throw() {
    SetLastError(ERROR_NOT_CAPABLE);
    return FALSE;
  }

  // Console APIs.
  __override bool FileDescriptorIsDisplayed(int fd) throw() {
    return false;
  }
  __override unsigned GetColumnCount(DWORD nStdHandle) throw() {
    return 80;
  }
  __override unsigned GetConsoleOutputTextAttributes() throw() {
    return 0;
  }
  __override void SetConsoleOutputTextAttributes(unsigned) throw() {
    __debugbreak();
  }
  __override void ResetConsoleOutputTextAttributes() throw() {
    __debugbreak();
  }

  // CRT APIs - handles and file numbers can be mapped directly.
  HANDLE HandleFromFD(int fd) const {
    if (fd == STDOUT_FILENO) return StdOutHandle.Handle;
    if (fd == STDERR_FILENO) return StdErrHandle.Handle;
    return (HANDLE)(uintptr_t)(fd);
  }
  __override int open_osfhandle(intptr_t osfhandle, int flags) throw() {
    DxcArgsHandle H((HANDLE)osfhandle);
    if (H == StdOutHandle.Handle) return STDOUT_FILENO;
    if (H == StdErrHandle.Handle) return STDERR_FILENO;
    return (int)(intptr_t)H.Handle;
  }
  __override intptr_t get_osfhandle(int fd) throw() {
    return (intptr_t)HandleFromFD(fd);
  }
  __override int close(int fd) throw() {
    return 0;
  }
  __override long lseek(int fd, long offset, int origin) throw() {
    CComPtr<IStream> stream;
    GetStreamForFD(fd, &stream);
    if (stream == nullptr) {
      errno = EBADF;
      return -1;
    }

    LARGE_INTEGER li;
    li.LowPart = offset;
    li.HighPart = 0;
    ULARGE_INTEGER newOffset;
    HRESULT hr = stream->Seek(li, origin, &newOffset);
    if (FAILED(hr)) {
      errno = EINVAL;
      return -1;
    }

    return newOffset.LowPart;
  }
  __override int setmode(int fd, int mode) throw() {
    return 0;
  }
  __override errno_t resize_file(_In_ LPCWSTR path, uint64_t size) throw() {
    return 0;
  }
  __override int Read(int fd, _Out_bytecap_(count) void* buffer, unsigned int count) throw() {
    CComPtr<IStream> stream;
    GetStreamForFD(fd, &stream);
    if (stream == nullptr) {
      errno = EBADF;
      return -1;
    }

    ULONG cbRead;
    HRESULT hr = stream->Read(buffer, count, &cbRead);
    if (FAILED(hr)) {
      errno = EIO;
      return -1;
    }

    return (int)cbRead;
  }
  __override int Write(int fd, _In_bytecount_(count) const void* buffer, unsigned int count) throw() {
    CComPtr<IStream> stream;
    GetStreamForFD(fd, &stream);
    if (stream == nullptr) {
      errno = EBADF;
      return -1;
    }

#ifdef _DEBUG
    if (fd == STDERR_FILENO) {
        char* copyWithNull = new char[count+1];
        strncpy(copyWithNull, (char*)buffer, count);
        copyWithNull[count] = '\0';
        OutputDebugStringA(copyWithNull);
        delete[] copyWithNull;
    }
#endif

    ULONG written;
    HRESULT hr = stream->Write(buffer, count, &written);
    if (FAILED(hr)) {
      errno = EIO;
      return -1;
    }

    return (int)written;
  }
};

static HRESULT
CreateDxcArgsFileSystem(_In_ IDxcBlob *pSource, _In_ LPCWSTR pSourceName,
                        _In_opt_ IDxcIncludeHandler *pIncludeHandler,
                        _Outptr_ DxcArgsFileSystem **ppResult) throw() {
  *ppResult = new (std::nothrow) DxcArgsFileSystem(pSource, pSourceName, pIncludeHandler);
  if (*ppResult == nullptr) {
    return E_OUTOFMEMORY;
  }
  return S_OK;
}

static void CreateOperationResultFromOutputs(
    IDxcBlob *pResultBlob, DxcArgsFileSystem *msfPtr,
    const std::string &warnings, clang::DiagnosticsEngine &diags,
    _COM_Outptr_ IDxcOperationResult **ppResult) {
  CComPtr<IStream> pErrorStream;
  CComPtr<IDxcBlobEncoding> pErrorBlob;
  msfPtr->GetStreamForHandle(StdOutHandle.Handle, &pErrorStream);

  dxcutil::CreateOperationResultFromOutputs(pResultBlob, pErrorStream, warnings,
                                            diags.hasErrorOccurred(), ppResult);
}

static void CreateOperationResultFromOutputs(
    AbstractMemoryStream *pOutputStream, DxcArgsFileSystem *msfPtr,
    const std::string &warnings, clang::DiagnosticsEngine &diags,
    _COM_Outptr_ IDxcOperationResult **ppResult) {
  CComPtr<IDxcBlob> pResultBlob;
  IFT(pOutputStream->QueryInterface(&pResultBlob));
  CreateOperationResultFromOutputs(pResultBlob, msfPtr, warnings, diags,
                                   ppResult);
}

class HLSLExtensionsCodegenHelperImpl : public HLSLExtensionsCodegenHelper {
private:
  CompilerInstance &m_CI;
  DxcLangExtensionsHelper &m_langExtensionsHelper;
  std::string m_rootSigDefine;

  // The metadata format is a root node that has pointers to metadata
  // nodes for each define. The metatdata node for a define is a pair
  // of (name, value) metadata strings.
  //
  // Example:
  // !hlsl.semdefs = {!0, !1}
  // !0 = !{!"FOO", !"BAR"}
  // !1 = !{!"BOO", !"HOO"}
  void WriteSemanticDefines(llvm::Module *M, const ParsedSemanticDefineList &defines) {
    // Create all metadata nodes for each define. Each node is a (name, value) pair.
    std::vector<MDNode *> mdNodes;
    for (const ParsedSemanticDefine &define : defines) {
      MDString *name  = MDString::get(M->getContext(), define.Name);
      MDString *value = MDString::get(M->getContext(), define.Value);
      mdNodes.push_back(MDNode::get(M->getContext(), { name, value }));
    }

    // Add root node with pointers to all define metadata nodes.
    NamedMDNode *Root = M->getOrInsertNamedMetadata(m_langExtensionsHelper.GetSemanticDefineMetadataName());
    for (MDNode *node : mdNodes)
      Root->addOperand(node);
  }

  SemanticDefineErrorList GetValidatedSemanticDefines(const ParsedSemanticDefineList &defines, ParsedSemanticDefineList &validated, SemanticDefineErrorList &errors) {
    for (const ParsedSemanticDefine &define : defines) {
      DxcLangExtensionsHelper::SemanticDefineValidationResult result = m_langExtensionsHelper.ValidateSemanticDefine(define.Name, define.Value);
        if (result.HasError())
          errors.emplace_back(SemanticDefineError(define.Location, SemanticDefineError::Level::Error, result.Error));
        if (result.HasWarning())
          errors.emplace_back(SemanticDefineError(define.Location, SemanticDefineError::Level::Warning, result.Warning));
        if (!result.HasError())
          validated.emplace_back(define);
    }

    return errors;
  }

public:
  HLSLExtensionsCodegenHelperImpl(CompilerInstance &CI, DxcLangExtensionsHelper &langExtensionsHelper, StringRef rootSigDefine)
  : m_CI(CI), m_langExtensionsHelper(langExtensionsHelper)
  , m_rootSigDefine(rootSigDefine)
  {}

  // Write semantic defines as metadata in the module.
  virtual std::vector<SemanticDefineError> WriteSemanticDefines(llvm::Module *M) override {
    // Grab the semantic defines seen by the parser.
    ParsedSemanticDefineList defines =
      CollectSemanticDefinesParsedByCompiler(m_CI, &m_langExtensionsHelper);

    // Nothing to do if we have no defines.
    SemanticDefineErrorList errors;
    if (!defines.size())
      return errors;

    ParsedSemanticDefineList validated;
    GetValidatedSemanticDefines(defines, validated, errors);
    WriteSemanticDefines(M, validated);
    return errors;
  }

  virtual std::string GetIntrinsicName(UINT opcode) override {
    return m_langExtensionsHelper.GetIntrinsicName(opcode);
  }
  
  virtual bool GetDxilOpcode(UINT opcode, OP::OpCode &dxilOpcode) override {
    UINT dop = static_cast<UINT>(OP::OpCode::NumOpCodes);
    if (m_langExtensionsHelper.GetDxilOpCode(opcode, dop)) {
      if (dop < static_cast<UINT>(OP::OpCode::NumOpCodes)) {
        dxilOpcode = static_cast<OP::OpCode>(dop);
        return true;
      }
    }
    return false;
  }

  virtual HLSLExtensionsCodegenHelper::CustomRootSignature::Status GetCustomRootSignature(CustomRootSignature *out) {
    // Find macro definition in preprocessor.
    Preprocessor &pp = m_CI.getPreprocessor();
    MacroInfo *macro = MacroExpander::FindMacroInfo(pp, m_rootSigDefine);
    if (!macro)
      return CustomRootSignature::NOT_FOUND;

    // Combine tokens into single string
    MacroExpander expander(pp, MacroExpander::STRIP_QUOTES);
    if (!expander.ExpandMacro(macro, &out->RootSignature))
      return CustomRootSignature::NOT_FOUND;

    // Record source location of root signature macro.
    out->EncodedSourceLocation = macro->getDefinitionLoc().getRawEncoding();

    return CustomRootSignature::FOUND;
  }
};

class DxcCompiler : public IDxcCompiler2, public IDxcLangExtensions, public IDxcContainerEvent, public IDxcVersionInfo {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  DxcLangExtensionsHelper m_langExtensionsHelper;
  CComPtr<IDxcContainerEventsHandler> m_pDxcContainerEventsHandler;

  void CreateDefineStrings(_In_count_(defineCount) const DxcDefine *pDefines,
                           UINT defineCount,
                           std::vector<std::string> &defines) {
    // Not very efficient but also not very important.
    for (UINT32 i = 0; i < defineCount; i++) {
      CW2A utf8Name(pDefines[i].Name, CP_UTF8);
      CW2A utf8Value(pDefines[i].Value, CP_UTF8);
      std::string val(utf8Name.m_psz);
      val += "=";
      val += (pDefines[i].Value) ? utf8Value.m_psz : "1";
      defines.push_back(val);
    }
  }

  void ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                           hlsl::options::DxcOpts &opts,
                           AbstractMemoryStream *pOutputStream,
                           _COM_Outptr_ IDxcOperationResult **ppResult,
                           bool &finished) {
    const llvm::opt::OptTable *table = ::options::getHlslOptTable();
    raw_stream_ostream outStream(pOutputStream);
    if (0 != hlsl::options::ReadDxcOpts(table, hlsl::options::CompilerFlags,
                                        mainArgs, opts, outStream)) {
      CComPtr<IDxcBlob> pErrorBlob;
      IFT(pOutputStream->QueryInterface(&pErrorBlob));
      CComPtr<IDxcBlobEncoding> pErrorBlobWithEncoding;
      outStream.flush();
      IFT(DxcCreateBlobWithEncodingSet(pErrorBlob.p, CP_UTF8,
                                       &pErrorBlobWithEncoding));
      IFT(DxcOperationResult::CreateFromResultErrorStatus(nullptr, pErrorBlobWithEncoding.p, E_INVALIDARG, ppResult));
      finished = true;
      return;
    }
    DXASSERT(!opts.HLSL2015, "else ReadDxcOpts didn't fail for non-isense");
    finished = false;
  }
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langExtensionsHelper)

  __override HRESULT STDMETHODCALLTYPE RegisterDxilContainerEventHandler(IDxcContainerEventsHandler *pHandler, UINT64 *pCookie) {
    DXASSERT(m_pDxcContainerEventsHandler == nullptr, "else events handler is already registered");
    *pCookie = 1; // Only one EventsHandler supported 
    m_pDxcContainerEventsHandler = pHandler;
    return S_OK;
  };
  __override HRESULT STDMETHODCALLTYPE UnRegisterDxilContainerEventHandler(UINT64 cookie) {
    DXASSERT(m_pDxcContainerEventsHandler != nullptr, "else unregister should not have been called");
    m_pDxcContainerEventsHandler.Release();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcCompiler,
                                 IDxcCompiler2,
                                 IDxcLangExtensions,
                                 IDxcContainerEvent,
                                 IDxcVersionInfo>
                                 (this, iid, ppvObject);
  }

  // Compile a single entry point to the target shader model
  __override HRESULT STDMETHODCALLTYPE Compile(
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
    _In_ LPCWSTR pEntryPoint,                     // entry point name
    _In_ LPCWSTR pTargetProfile,                  // shader profile to compile
    _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult   // Compiler output status, buffer, and errors
  ) {
    return CompileWithDebug(pSource, pSourceName, pEntryPoint, pTargetProfile,
                            pArguments, argCount, pDefines, defineCount,
                            pIncludeHandler, ppResult, nullptr, nullptr);
  }

  // Compile a single entry point to the target shader model with debug information.
  __override HRESULT STDMETHODCALLTYPE CompileWithDebug(
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
    _In_ LPCWSTR pEntryPoint,                     // Entry point name
    _In_ LPCWSTR pTargetProfile,                  // Shader profile to compile
    _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult,  // Compiler output status, buffer, and errors
    _Outptr_opt_result_z_ LPWSTR *ppDebugBlobName,// Suggested file name for debug blob.
    _COM_Outptr_opt_ IDxcBlob **ppDebugBlob       // Debug blob
  ) {
    if (pSource == nullptr || ppResult == nullptr ||
        (defineCount > 0 && pDefines == nullptr) ||
        (argCount > 0 && pArguments == nullptr) || pEntryPoint == nullptr ||
        pTargetProfile == nullptr)
      return E_INVALIDARG;
    *ppResult = nullptr;
    AssignToOutOpt(nullptr, ppDebugBlobName);
    AssignToOutOpt(nullptr, ppDebugBlob);

    HRESULT hr = S_OK;
    CComPtr<IDxcBlobEncoding> utf8Source;
    CComPtr<AbstractMemoryStream> pOutputStream;
    CHeapPtr<wchar_t> DebugBlobName;
    DxcEtw_DXCompilerCompile_Start();
    IFC(hlsl::DxcGetBlobAsUtf8(pSource, &utf8Source));

    try {
      CComPtr<IMalloc> pMalloc;
      CComPtr<IDxcBlob> pOutputBlob;
      DxcArgsFileSystem *msfPtr;
      IFT(CreateDxcArgsFileSystem(utf8Source, pSourceName, pIncludeHandler, &msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      IFT(CoGetMalloc(1, &pMalloc));
      IFT(CreateMemoryStream(pMalloc, &pOutputStream));
      IFT(pOutputStream.QueryInterface(&pOutputBlob));

      int argCountInt;
      IFT(UIntToInt(argCount, &argCountInt));
      hlsl::options::MainArgs mainArgs(argCountInt, pArguments, 0);
      hlsl::options::DxcOpts opts;
      bool finished;
      ReadOptsAndValidate(mainArgs, opts, pOutputStream, ppResult, finished);
      if (finished) {
        hr = S_OK;
        goto Cleanup;
      }
      if (opts.DisplayIncludeProcess)
        msfPtr->EnableDisplayIncludeProcess();

      // Prepare UTF8-encoded versions of API values.
      CW2A pUtf8EntryPoint(pEntryPoint, CP_UTF8);
      CW2A pUtf8TargetProfile(pTargetProfile, CP_UTF8);
      CW2A utf8SourceName(pSourceName, CP_UTF8);
      const char *pUtf8SourceName = utf8SourceName.m_psz;
      if (pUtf8SourceName == nullptr) {
        if (opts.InputFile.empty()) {
          pUtf8SourceName = "input.hlsl";
        }
        else {
          pUtf8SourceName = opts.InputFile.data();
        }
      }

      IFT(msfPtr->RegisterOutputStream(L"output.bc", pOutputStream));
      IFT(msfPtr->CreateStdStreams(pMalloc));

      StringRef Data((LPSTR)utf8Source->GetBufferPointer(),
                     utf8Source->GetBufferSize());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(
          llvm::MemoryBuffer::getMemBufferCopy(Data, pUtf8SourceName));

      // Not very efficient but also not very important.
      std::vector<std::string> defines;
      CreateDefineStrings(pDefines, defineCount, defines);
      CreateDefineStrings(opts.Defines.data(), opts.Defines.size(), defines);

      // Setup a compiler instance.
      std::string warnings;
      raw_string_ostream w(warnings);
      raw_stream_ostream outStream(pOutputStream.p);
      CompilerInstance compiler;
      std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
          std::make_unique<TextDiagnosticPrinter>(w, &compiler.getDiagnosticOpts());
      SetupCompilerForCompile(compiler, &m_langExtensionsHelper, utf8SourceName, diagPrinter.get(), defines, opts, pArguments, argCount);
      msfPtr->SetupForCompilerInstance(compiler);

      // The clang entry point (cc1_main) would now create a compiler invocation
      // from arguments, but for this path we're exclusively trying to compile
      // to LLVM bitcode and then package that into a DXBC blob.
      //
      // With the compiler invocation built from command line arguments, the
      // next step is to call ExecuteCompilerInvocation, which creates a
      // FrontendAction* of EmitBCAction, which is a CodeGenAction, which is an
      // ASTFrontendAction. That sets up a BackendConsumer as the ASTConsumer.
      compiler.getFrontendOpts().OutputFile = "output.bc";
      compiler.WriteDefaultOutputDirectly = true;
      compiler.setOutStream(&outStream);

      compiler.getLangOpts().HLSLEntryFunction =
      compiler.getCodeGenOpts().HLSLEntryFunction = pUtf8EntryPoint.m_psz;
      compiler.getCodeGenOpts().HLSLProfile = pUtf8TargetProfile.m_psz;

      unsigned rootSigMajor = 0;
      unsigned rootSigMinor = 0;
      if (compiler.getCodeGenOpts().HLSLProfile == "rootsig_1_1") {
        rootSigMajor = 1;
        rootSigMinor = 1;
      } else if (compiler.getCodeGenOpts().HLSLProfile == "rootsig_1_0") {
        rootSigMajor = 1;
        rootSigMinor = 0;
      }
      compiler.getLangOpts().IsHLSLLibrary = opts.IsLibraryProfile();

      // NOTE: this calls the validation component from dxil.dll; the built-in
      // validator can be used as a fallback.
      bool produceFullContainer = !opts.CodeGenHighLevel && !opts.AstDump && !opts.OptDump && rootSigMajor == 0;

      bool needsValidation = produceFullContainer && !opts.DisableValidation &&
                             !opts.IsLibraryProfile();

      if (needsValidation) {
        UINT32 majorVer, minorVer;
        dxcutil::GetValidatorVersion(&majorVer, &minorVer);
        compiler.getCodeGenOpts().HLSLValidatorMajorVer = majorVer;
        compiler.getCodeGenOpts().HLSLValidatorMinorVer = minorVer;
      }

      if (opts.AstDump) {
        clang::ASTDumpAction dumpAction;
        // Consider - ASTDumpFilter, ASTDumpLookups
        compiler.getFrontendOpts().ASTDumpDecls = true;
        FrontendInputFile file(utf8SourceName.m_psz, IK_HLSL);
        dumpAction.BeginSourceFile(compiler, file);
        dumpAction.Execute();
        dumpAction.EndSourceFile();
        outStream.flush();
      }
      else if (opts.OptDump) {
        llvm::LLVMContext llvmContext;
        EmitOptDumpAction action(&llvmContext);
        FrontendInputFile file(utf8SourceName.m_psz, IK_HLSL);
        action.BeginSourceFile(compiler, file);
        action.Execute();
        action.EndSourceFile();
        outStream.flush();
      }
      else if (rootSigMajor) {
        HLSLRootSignatureAction action(
            compiler.getCodeGenOpts().HLSLEntryFunction, rootSigMajor,
            rootSigMinor);
        FrontendInputFile file(utf8SourceName.m_psz, IK_HLSL);
        action.BeginSourceFile(compiler, file);
        action.Execute();
        action.EndSourceFile();
        outStream.flush();
        // Don't do work to put in a container if an error has occurred
        bool compileOK = !compiler.getDiagnostics().hasErrorOccurred();
        if (compileOK) {
          auto rootSigHandle = action.takeRootSigHandle();

          CComPtr<AbstractMemoryStream> pContainerStream;
          IFT(CreateMemoryStream(pMalloc, &pContainerStream));
          SerializeDxilContainerForRootSignature(rootSigHandle.get(),
                                                 pContainerStream);

          pOutputBlob.Release();
          IFT(pContainerStream.QueryInterface(&pOutputBlob));
        }
      }
      // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
      else if (opts.GenSPIRV) {
          clang::EmitSPIRVAction action;
          FrontendInputFile file(utf8SourceName.m_psz, IK_HLSL);
          action.BeginSourceFile(compiler, file);
          action.Execute();
          action.EndSourceFile();
          outStream.flush();
      }
#endif
      // SPIRV change ends
      else {
        llvm::LLVMContext llvmContext;
        EmitBCAction action(&llvmContext);
        FrontendInputFile file(utf8SourceName.m_psz, IK_HLSL);
        bool compileOK;
        if (action.BeginSourceFile(compiler, file)) {
          action.Execute();
          action.EndSourceFile();
          compileOK = !compiler.getDiagnostics().hasErrorOccurred();
        }
        else {
          compileOK = false;
        }
        outStream.flush();

        SerializeDxilFlags SerializeFlags = SerializeDxilFlags::None;
        if (opts.DebugInfo) {
          SerializeFlags = SerializeDxilFlags::IncludeDebugNamePart;
          // Unless we want to strip it right away, include it in the container.
          if (!opts.StripDebug || ppDebugBlob == nullptr) {
            SerializeFlags |= SerializeDxilFlags::IncludeDebugInfoPart;
          }
        }
        if (opts.DebugNameForSource) {
          SerializeFlags |= SerializeDxilFlags::DebugNameDependOnSource;
        }

        // Don't do work to put in a container if an error has occurred
        // Do not create a container when there is only a a high-level representation in the module.
        if (compileOK && !opts.CodeGenHighLevel) {
          HRESULT valHR = S_OK;

          if (needsValidation) {
            valHR = dxcutil::ValidateAndAssembleToContainer(
                action.takeModule(), pOutputBlob, pMalloc, SerializeFlags,
                pOutputStream, opts.DebugInfo, compiler.getDiagnostics());
          } else {
            dxcutil::AssembleToContainer(action.takeModule(),
                                                 pOutputBlob, pMalloc,
                                                 SerializeFlags, pOutputStream);
          }

          // Callback after valid DXIL is produced
          if (SUCCEEDED(valHR)) {
            CComPtr<IDxcBlob> pTargetBlob;
            if (m_pDxcContainerEventsHandler != nullptr) {
              HRESULT hr = m_pDxcContainerEventsHandler->OnDxilContainerBuilt(pOutputBlob, &pTargetBlob);
              if (SUCCEEDED(hr) && pTargetBlob != nullptr) {
                std::swap(pOutputBlob, pTargetBlob);
              }
            }

            if (ppDebugBlobName && produceFullContainer) {
              const DxilContainerHeader *pContainer = reinterpret_cast<DxilContainerHeader *>(pOutputBlob->GetBufferPointer());
              DXASSERT(IsValidDxilContainer(pContainer, pOutputBlob->GetBufferSize()), "else invalid container generated");
              auto it = std::find_if(begin(pContainer), end(pContainer),
                DxilPartIsType(DFCC_ShaderDebugName));
              if (it != end(pContainer)) {
                const char *pDebugName;
                if (GetDxilShaderDebugName(*it, &pDebugName, nullptr) && pDebugName && *pDebugName) {
                  IFTBOOL(Unicode::UTF8BufferToUTF16ComHeap(pDebugName, &DebugBlobName), DXC_E_CONTAINER_INVALID);
                }
              }
            }
          }
        }
      }

      // Add std err to warnings.
      msfPtr->WriteStdErrToStream(w);

      CreateOperationResultFromOutputs(pOutputBlob, msfPtr, warnings,
                                       compiler.getDiagnostics(), ppResult);

      // On success, return values. After assigning ppResult, nothing should fail.
      HRESULT status;
      DXVERIFY_NOMSG(SUCCEEDED((*ppResult)->GetStatus(&status)));
      if (SUCCEEDED(status)) {
        if (opts.DebugInfo && ppDebugBlob) {
          DXVERIFY_NOMSG(SUCCEEDED(pOutputStream.QueryInterface(ppDebugBlob)));
        }
        if (ppDebugBlobName) {
          *ppDebugBlobName = DebugBlobName.Detach();
        }
      }

      hr = S_OK;
    }
    CATCH_CPP_ASSIGN_HRESULT();
  Cleanup:
    DxcEtw_DXCompilerCompile_Stop(hr);
    return hr;
  }

  // Preprocess source text
  __override HRESULT STDMETHODCALLTYPE Preprocess(
    _In_ IDxcBlob *pSource,                       // Source text to preprocess
    _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
    _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult   // Preprocessor output status, buffer, and errors
    ) {
    if (pSource == nullptr || ppResult == nullptr ||
        (defineCount > 0 && pDefines == nullptr) ||
        (argCount > 0 && pArguments == nullptr))
      return E_INVALIDARG;
    *ppResult = nullptr;

    HRESULT hr = S_OK;
    DxcEtw_DXCompilerPreprocess_Start();
    CComPtr<IDxcBlobEncoding> utf8Source;
    IFC(hlsl::DxcGetBlobAsUtf8(pSource, &utf8Source));

    try {
      CComPtr<IMalloc> pMalloc;
      CComPtr<AbstractMemoryStream> pOutputStream;
      DxcArgsFileSystem *msfPtr;
      IFT(CreateDxcArgsFileSystem(utf8Source, pSourceName, pIncludeHandler, &msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      IFT(CoGetMalloc(1, &pMalloc));
      IFT(CreateMemoryStream(pMalloc, &pOutputStream));

      const llvm::opt::OptTable *table = ::options::getHlslOptTable();
      int argCountInt;
      IFT(UIntToInt(argCount, &argCountInt));
      hlsl::options::MainArgs mainArgs(argCountInt, pArguments, 0);
      hlsl::options::DxcOpts opts;
      bool finished;
      ReadOptsAndValidate(mainArgs, opts, pOutputStream, ppResult, finished);
      if (finished) {
        hr = S_OK;
        goto Cleanup;
      }

      // Prepare UTF8-encoded versions of API values.
      CW2A utf8SourceName(pSourceName, CP_UTF8);
      const char *pUtf8SourceName = utf8SourceName.m_psz;
      if (pUtf8SourceName == nullptr) {
        if (opts.InputFile.empty()) {
          pUtf8SourceName = "input.hlsl";
        }
        else {
          pUtf8SourceName = opts.InputFile.data();
        }
      }

      IFT(msfPtr->RegisterOutputStream(L"output.hlsl", pOutputStream));
      IFT(msfPtr->CreateStdStreams(pMalloc));

      StringRef Data((LPSTR)utf8Source->GetBufferPointer(),
        utf8Source->GetBufferSize());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(
        llvm::MemoryBuffer::getMemBufferCopy(Data, pUtf8SourceName));

      // Not very efficient but also not very important.
      std::vector<std::string> defines;
      CreateDefineStrings(pDefines, defineCount, defines);

      // Setup a compiler instance.
      std::string warnings;
      raw_string_ostream w(warnings);
      raw_stream_ostream outStream(pOutputStream.p);
      CompilerInstance compiler;
      std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
          std::make_unique<TextDiagnosticPrinter>(w, &compiler.getDiagnosticOpts());
      SetupCompilerForCompile(compiler, &m_langExtensionsHelper, utf8SourceName, diagPrinter.get(), defines, opts, pArguments, argCount);
      msfPtr->SetupForCompilerInstance(compiler);

      // The clang entry point (cc1_main) would now create a compiler invocation
      // from arguments, but for this path we're exclusively trying to preproces
      // to text.
      compiler.getFrontendOpts().OutputFile = "output.hlsl";
      compiler.WriteDefaultOutputDirectly = true;
      compiler.setOutStream(&outStream);

      // These settings are back-compatible with fxc.
      clang::PreprocessorOutputOptions &PPOutOpts =
          compiler.getPreprocessorOutputOpts();
      PPOutOpts.ShowCPP = 1;            // Print normal preprocessed output.
      PPOutOpts.ShowComments = 0;       // Show comments.
      PPOutOpts.ShowLineMarkers = 1;    // Show \#line markers.
      PPOutOpts.UseLineDirectives = 1;  // Use \#line instead of GCC-style \# N.
      PPOutOpts.ShowMacroComments = 0;  // Show comments, even in macros.
      PPOutOpts.ShowMacros = 0;         // Print macro definitions.
      PPOutOpts.RewriteIncludes = 0;    // Preprocess include directives only.

      FrontendInputFile file(utf8SourceName.m_psz, IK_HLSL);
      clang::PrintPreprocessedAction action;
      if (action.BeginSourceFile(compiler, file)) {
        action.Execute();
        action.EndSourceFile();
      }
      outStream.flush();

      // Add std err to warnings.
      msfPtr->WriteStdErrToStream(w);

      CreateOperationResultFromOutputs(pOutputStream, msfPtr, warnings,
        compiler.getDiagnostics(), ppResult);
      hr = S_OK;
    }
    CATCH_CPP_ASSIGN_HRESULT();
  Cleanup:
    DxcEtw_DXCompilerPreprocess_Stop(hr);
    return hr;
  }

  // Disassemble a shader.
  __override HRESULT STDMETHODCALLTYPE Disassemble(
    _In_ IDxcBlob *pProgram,                      // Program to disassemble.
    _COM_Outptr_ IDxcBlobEncoding** ppDisassembly // Disassembly text.
    ) {
    if (pProgram == nullptr || ppDisassembly == nullptr)
      return E_INVALIDARG;

    *ppDisassembly = nullptr;

    HRESULT hr = S_OK;
    DxcEtw_DXCompilerDisassemble_Start();
    try {
      ::llvm::sys::fs::MSFileSystem *msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      std::string StreamStr;
      raw_string_ostream Stream(StreamStr);

      IFC(dxcutil::Disassemble(pProgram, Stream));

      IFT(DxcCreateBlobWithEncodingOnHeapCopy(
          StreamStr.c_str(), StreamStr.size(), CP_UTF8, ppDisassembly));

      return S_OK;
    }
    CATCH_CPP_ASSIGN_HRESULT();
  Cleanup:
    DxcEtw_DXCompilerDisassemble_Stop(hr);
    return hr;
  }

  void SetupCompilerForCompile(CompilerInstance &compiler,
                               _In_ DxcLangExtensionsHelper *helper,
                               _In_ LPCSTR pMainFile, _In_ TextDiagnosticPrinter *diagPrinter,
                               _In_ std::vector<std::string>& defines,
                               _In_ hlsl::options::DxcOpts &Opts,
                               _In_count_(argCount) LPCWSTR *pArguments,
                               _In_ UINT32 argCount) {
    // Setup a compiler instance.
    std::shared_ptr<TargetOptions> targetOptions(new TargetOptions);
    targetOptions->Triple = "dxil-ms-dx";
    compiler.HlslLangExtensions = helper;
    compiler.createDiagnostics(diagPrinter, false);
    compiler.createFileManager();
    compiler.createSourceManager(compiler.getFileManager());
    compiler.setTarget(
        TargetInfo::CreateTargetInfo(compiler.getDiagnostics(), targetOptions));

    compiler.getFrontendOpts().Inputs.push_back(FrontendInputFile(pMainFile, IK_HLSL));
    // Setup debug information.
    if (Opts.DebugInfo) {
      CodeGenOptions &CGOpts = compiler.getCodeGenOpts();
      CGOpts.setDebugInfo(CodeGenOptions::FullDebugInfo);
      CGOpts.DebugColumnInfo = 1;
      CGOpts.DwarfVersion = 4; // Latest version.
      // TODO: consider
      // DebugPass, DebugCompilationDir, DwarfDebugFlags, SplitDwarfFile
    }

    clang::PreprocessorOptions &PPOpts(compiler.getPreprocessorOpts());
    for (size_t i = 0; i < defines.size(); ++i) {
      PPOpts.addMacroDef(defines[i]);
    }

    // Pick additional arguments.
    clang::HeaderSearchOptions &HSOpts = compiler.getHeaderSearchOpts();
    HSOpts.UseBuiltinIncludes = 0;
    // Consider: should we force-include '.' if the source file is relative?
    for (const llvm::opt::Arg *A : Opts.Args.filtered(options::OPT_I)) {
      const bool IsFrameworkFalse = false;
      const bool IgnoreSysRoot = true;
      if (IsAbsoluteOrCurDirRelative(A->getValue())) {
        HSOpts.AddPath(A->getValue(), frontend::Angled, IsFrameworkFalse, IgnoreSysRoot);
      }
      else {
        std::string s("./");
        s += A->getValue();
        HSOpts.AddPath(s, frontend::Angled, IsFrameworkFalse, IgnoreSysRoot);
      }
    }

    // Apply root signature option.
    unsigned rootSigMinor;
    if (Opts.ForceRootSigVer.empty() || Opts.ForceRootSigVer == "rootsig_1_1") {
      rootSigMinor = 1;
    }
    else {
      DXASSERT(Opts.ForceRootSigVer == "rootsig_1_0",
               "else opts should have been rejected");
      rootSigMinor = 0;
    }
    compiler.getLangOpts().RootSigMajor = 1;
    compiler.getLangOpts().RootSigMinor = rootSigMinor;
    compiler.getLangOpts().HLSL2015 = Opts.HLSL2015;
    compiler.getLangOpts().HLSL2016 = Opts.HLSL2016;
    compiler.getLangOpts().HLSL2017 = Opts.HLSL2017;

    if (Opts.WarningAsError)
      compiler.getDiagnostics().setWarningsAsErrors(true);

    if (Opts.IEEEStrict)
      compiler.getCodeGenOpts().UnsafeFPMath = true;

    if (Opts.DisableOptimizations)
      compiler.getCodeGenOpts().DisableLLVMOpts = true;

    compiler.getCodeGenOpts().OptimizationLevel = Opts.OptLevel;
    if (Opts.OptLevel >= 3)
      compiler.getCodeGenOpts().UnrollLoops = true;

    compiler.getCodeGenOpts().HLSLHighLevel = Opts.CodeGenHighLevel;
    compiler.getCodeGenOpts().HLSLAllResourcesBound = Opts.AllResourcesBound;
    compiler.getCodeGenOpts().HLSLDefaultRowMajor = Opts.DefaultRowMajor;
    compiler.getCodeGenOpts().HLSLPreferControlFlow = Opts.PreferFlowControl;
    compiler.getCodeGenOpts().HLSLAvoidControlFlow = Opts.AvoidFlowControl;
    compiler.getCodeGenOpts().HLSLNotUseLegacyCBufLoad = Opts.NotUseLegacyCBufLoad;
    compiler.getCodeGenOpts().HLSLDefines = defines;
    compiler.getCodeGenOpts().MainFileName = pMainFile;

    // Translate signature packing options
    if (Opts.PackPrefixStable)
      compiler.getCodeGenOpts().HLSLSignaturePackingStrategy = (unsigned)DXIL::PackingStrategy::PrefixStable;
    else if (Opts.PackOptimized)
      compiler.getCodeGenOpts().HLSLSignaturePackingStrategy = (unsigned)DXIL::PackingStrategy::Optimized;
    else
      compiler.getCodeGenOpts().HLSLSignaturePackingStrategy = (unsigned)DXIL::PackingStrategy::Default;

    // Constructing vector of wide strings to pass in to codegen. Just passing
    // in pArguments will expose ownership of memory to both CodeGenOptions and
    // this caller, which can lead to unexpected behavior.
    for (UINT32 i = 0; i != argCount; ++i) {
      compiler.getCodeGenOpts().HLSLArguments.emplace_back(
          Unicode::UTF16ToUTF8StringOrThrow(pArguments[i]));
    }
    // Overrding default set of loop unroll.
    if (Opts.PreferFlowControl)
      compiler.getCodeGenOpts().UnrollLoops = false;
    if (Opts.AvoidFlowControl)
      compiler.getCodeGenOpts().UnrollLoops = true;

    // always inline for hlsl
    compiler.getCodeGenOpts().setInlining(
        clang::CodeGenOptions::OnlyAlwaysInlining);

    compiler.getCodeGenOpts().HLSLExtensionsCodegen = std::make_shared<HLSLExtensionsCodegenHelperImpl>(compiler, m_langExtensionsHelper, Opts.RootSignatureDefine);
  }

  // IDxcVersionInfo
  __override HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor) {
    if (pMajor == nullptr || pMinor == nullptr)
      return E_INVALIDARG;
    *pMajor = DXIL::kDxilMajor;
    *pMinor = DXIL::kDxilMinor;
    return S_OK;
  }
  __override HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags) {
    if (pFlags == nullptr)
      return E_INVALIDARG;
    *pFlags = DxcVersionInfoFlags_None;
#ifdef _DEBUG
    *pFlags |= DxcVersionInfoFlags_Debug;
#endif
    return S_OK;
  }

};

HRESULT CreateDxcCompiler(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcCompiler> result = new (std::nothrow) DxcCompiler();
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
