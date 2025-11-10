//===- WinAdapter.h - Windows Adapter for non-Windows platforms -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines Windows-specific types, macros, and SAL annotations used
// in the codebase for non-Windows platforms.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_WIN_ADAPTER_H
#define LLVM_SUPPORT_WIN_ADAPTER_H

#ifndef _WIN32

#ifdef __cplusplus
#include <atomic>
#include <cassert>
#include <climits>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>
#endif // __cplusplus

#include "../winadapter.h" //To avoid duplicates, some things are defined by WSL, some by dxc

#define COM_NO_WINDOWS_H // needed to inform d3d headers that this isn't windows

//===----------------------------------------------------------------------===//
//
//                             Begin: Macro Definitions
//
//===----------------------------------------------------------------------===//
#define ATLASSERT assert

#define CoTaskMemAlloc malloc
#define CoTaskMemFree free

#define ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))

// If it is GCC, there is no UUID support and we must emulate it.
// Clang support depends on the -fms-extensions compiler flag.
#if !defined(__clang__) || !defined(_MSC_EXTENSIONS)
#define __EMULATE_UUID 1
#endif // __clang__

#ifdef __EMULATE_UUID
#define __declspec(x)
#endif // __EMULATE_UUID

#define DECLSPEC_SELECTANY

#ifdef __EMULATE_UUID
#define uuid(id)
#endif // __EMULATE_UUID

#define STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#define STDMETHODIMP STDMETHODIMP_(HRESULT)

#define RtlEqualMemory(Destination, Source, Length)                            \
  (!memcmp((Destination), (Source), (Length)))
#define RtlMoveMemory(Destination, Source, Length)                             \
  memmove((Destination), (Source), (Length))
#define RtlCopyMemory(Destination, Source, Length)                             \
  memcpy((Destination), (Source), (Length))
#define RtlFillMemory(Destination, Length, Fill)                               \
  memset((Destination), (Fill), (Length))
#define RtlZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#define MoveMemory RtlMoveMemory
#define CopyMemory RtlCopyMemory
#define FillMemory RtlFillMemory
#define ZeroMemory RtlZeroMemory

// We ignore the code page completely on Linux.
#define GetConsoleOutputCP() 0

#define _HRESULT_TYPEDEF_(_sc) ((HRESULT)_sc)
#define DISP_E_BADINDEX _HRESULT_TYPEDEF_(0x8002000BL)
#define REGDB_E_CLASSNOTREG _HRESULT_TYPEDEF_(0x80040154L)

// This is an unsafe conversion. If needed, we can later implement a safe
// conversion that throws exceptions for overflow cases.
#define UIntToInt(uint_arg, int_ptr_arg) *int_ptr_arg = uint_arg

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

// Use errno to implement {Get|Set}LastError
#define GetLastError() errno
#define SetLastError(ERR) errno = ERR

// Map these errors to equivalent errnos.
#define ERROR_SUCCESS 0L
#define ERROR_ARITHMETIC_OVERFLOW EOVERFLOW
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_FUNCTION_NOT_CALLED ENOSYS
#define ERROR_IO_DEVICE EIO
#define ERROR_INSUFFICIENT_BUFFER ENOBUFS
#define ERROR_INVALID_HANDLE EBADF
#define ERROR_INVALID_PARAMETER EINVAL
#define ERROR_OUT_OF_STRUCTURES ENOMEM
#define ERROR_NOT_CAPABLE EPERM
#define ERROR_NOT_FOUND ENOTSUP
#define ERROR_UNHANDLED_EXCEPTION EBADF
#define ERROR_BROKEN_PIPE EPIPE

// Used by HRESULT <--> WIN32 error code conversion
#define SEVERITY_ERROR 1
#define FACILITY_WIN32 7
#define HRESULT_CODE(hr) ((hr) & 0xFFFF)
#define MAKE_HRESULT(severity, facility, code)                                 \
  ((HRESULT)(((unsigned long)(severity) << 31) |                               \
             ((unsigned long)(facility) << 16) | ((unsigned long)(code))))

#define FILE_TYPE_UNKNOWN 0x0000
#define FILE_TYPE_DISK 0x0001
#define FILE_TYPE_CHAR 0x0002
#define FILE_TYPE_PIPE 0x0003
#define FILE_TYPE_REMOTE 0x8000

#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// STGTY ENUMS
#define STGTY_STORAGE 1
#define STGTY_STREAM 2
#define STGTY_LOCKBYTES 3
#define STGTY_PROPERTY 4

// Storage errors
#define STG_E_INVALIDFUNCTION 1L
#define STG_E_ACCESSDENIED 2L

#define STREAM_SEEK_SET 0
#define STREAM_SEEK_CUR 1
#define STREAM_SEEK_END 2

#define HEAP_NO_SERIALIZE 0x1
#define HEAP_ZERO_MEMORY 0x8

#define MB_ERR_INVALID_CHARS 0x00000008 // error for invalid chars

// File IO

#define CREATE_ALWAYS 2
#define CREATE_NEW 1
#define OPEN_ALWAYS 4
#define OPEN_EXISTING 3
#define TRUNCATE_EXISTING 5

#define FILE_SHARE_DELETE 0x00000004
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002

#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000

#define _atoi64 atoll
#define sprintf_s snprintf
#define _strdup strdup
#define _strnicmp strnicmp

#define vsnprintf_s vsnprintf
#define strcat_s strcat
#define strcpy_s(dst, n, src) strncpy(dst, src, n)
#define _vscwprintf vwprintf
#define vswprintf_s vswprintf
#define swprintf_s swprintf

#define StringCchCopyW(dst, n, src) wcsncpy(dst, src, n)

#define OutputDebugStringW(msg) fputws(msg, stderr)

#define OutputDebugStringA(msg) fputs(msg, stderr)
#define OutputDebugFormatA(...) fprintf(stderr, __VA_ARGS__)

// Event Tracing for Windows (ETW) provides application programmers the ability
// to start and stop event tracing sessions, instrument an application to
// provide trace events, and consume trace events.
#define DxcEtw_DXCompilerCreateInstance_Start()
#define DxcEtw_DXCompilerCreateInstance_Stop(hr)
#define DxcEtw_DXCompilerCompile_Start()
#define DxcEtw_DXCompilerCompile_Stop(hr)
#define DxcEtw_DXCompilerDisassemble_Start()
#define DxcEtw_DXCompilerDisassemble_Stop(hr)
#define DxcEtw_DXCompilerPreprocess_Start()
#define DxcEtw_DXCompilerPreprocess_Stop(hr)
#define DxcEtw_DxcValidation_Start()
#define DxcEtw_DxcValidation_Stop(hr)

#define UInt32Add UIntAdd
#define Int32ToUInt32 IntToUInt

//===--------------------- HRESULT Related Macros -------------------------===//

#define E_BOUNDS (HRESULT)0x8000000B
#define E_NOT_VALID_STATE (HRESULT)0x8007139F

#define DXC_FAILED(hr) (((HRESULT)(hr)) < 0)

#define HRESULT_FROM_WIN32(x)                                                  \
  (HRESULT)(x) <= 0 ? (HRESULT)(x)                                             \
                    : (HRESULT)(((x) & 0x0000FFFF) | (7 << 16) | 0x80000000)

#define DXC_FAILED(hr) (((HRESULT)(hr)) < 0)

//===----------------------------------------------------------------------===//
//
//                         Begin: Disable SAL Annotations
//
//===----------------------------------------------------------------------===//

#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_z_
#define _Outptr_opt_result_z_
#define _Outptr_result_maybenull_
#define _Outptr_result_nullonfailure_
#define _Outptr_result_buffer_maybenull_(ptr)
#define _Outptr_result_buffer_(ptr)

#define _Maybenull_

#define __debugbreak()

// GCC produces erros on calling convention attributes.
#ifdef __GNUC__
#define __cdecl
#define __CRTDECL
#define __stdcall
#define __vectorcall
#define __thiscall
#define __fastcall
#define __clrcall
#endif // __GNUC__

//===----------------------------------------------------------------------===//
//
//                             Begin: Type Definitions
//
//===----------------------------------------------------------------------===//

#ifdef __cplusplus

// Note: using fixed-width here to match Windows widths
// Specifically this is different for 'long' vs 'LONG'
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32, UINT, ULONG, DWORD, WINBOOL, BOOL;
typedef int32_t INT32, INT, LONG;
typedef uint64_t UINT64, ULONG_PTR;
typedef int64_t INT64, LONG_PTR;
typedef void VOID, *HANDLE, *RPC_IF_HANDLE, *LPVOID;
typedef const void *LPCVOID;
typedef size_t SIZE_T;
typedef float FLOAT;
typedef double DOUBLE;
typedef unsigned char BYTE;
typedef int HWND;
typedef int PALETTEENTRY;
typedef int HDC;
typedef uint16_t WORD;
typedef void *PVOID;
typedef char BOOLEAN;
typedef uint64_t ULONGLONG;
typedef uint16_t USHORT, *PUSHORT;
typedef int64_t LONGLONG, *PLONGLONG;
typedef int64_t LONG_PTR, *PLONG_PTR;
typedef int64_t LONG64, *PLONG64;
typedef uint64_t ULONG64, *PULONG64;
typedef wchar_t WCHAR, *PWSTR;
typedef uint8_t UCHAR, *PUCHAR;
typedef uint64_t ULONG_PTR, *PULONG_PTR;
typedef uint64_t UINT_PTR, *PUINT_PTR;
typedef int64_t INT_PTR, *PINT_PTR;

// Note: WCHAR is not the same between Windows and Linux, to enable
// string manipulation APIs to work with resulting strings.
// APIs to D3D/DXCore will work on Linux wchars, but beware with
// interactions directly with the Windows kernel.
typedef char CHAR, *PSTR, *LPSTR, TCHAR, *PTSTR;
typedef const char *LPCSTR, *PCSTR, *LPCTSTR, *PCTSTR;
typedef wchar_t WCHAR, *PWSTR, *LPWSTR, *PWCHAR;
typedef const wchar_t *LPCWSTR, *PCWSTR;

typedef BYTE *LPBYTE;
typedef BOOLEAN *PBOOLEAN;

typedef BOOL *LPBOOL;

typedef DWORD *LPDWORD;

typedef INT8 *PINT8;
typedef INT32 *PINT32;

typedef int errno_t;

typedef WCHAR OLECHAR;
typedef OLECHAR *BSTR;
typedef OLECHAR *LPOLESTR;

typedef std::nullptr_t nullptr_t;

typedef signed int HRESULT;

//===--------------------- Handle Types -----------------------------------===//

#define DECLARE_HANDLE(name)                                                   \
  struct name##__ {                                                            \
    int unused;                                                                \
  };                                                                           \
  typedef struct name##__ *name
DECLARE_HANDLE(HINSTANCE);

typedef void *HMODULE;

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)

//===--------------------- ID Types and Macros for COM --------------------===//

typedef const GUID &REFGUID;
typedef const GUID &REFCLSID;

typedef IID *LPIID;
typedef const IID &REFIID;

inline bool IsEqualIID(REFIID riid1, REFIID riid2) {
  return IsEqualGUID(riid1, riid2);
}

inline bool IsEqualCLSID(REFCLSID rclsid1, REFCLSID rclsid2) {
  return IsEqualGUID(rclsid1, rclsid2);
}

//===--------------------- Struct Types -----------------------------------===//

typedef struct _FILETIME {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _BY_HANDLE_FILE_INFORMATION {
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD dwVolumeSerialNumber;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD nNumberOfLinks;
  DWORD nFileIndexHigh;
  DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION,
    *LPBY_HANDLE_FILE_INFORMATION;

typedef struct _WIN32_FIND_DATAW {
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  WCHAR cFileName[260];
  WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

typedef struct tagSTATSTG {
  LPOLESTR pwcsName;
  DWORD type;
  ULARGE_INTEGER cbSize;
  FILETIME mtime;
  FILETIME ctime;
  FILETIME atime;
  DWORD grfMode;
  DWORD grfLocksSupported;
  CLSID clsid;
  DWORD grfStateBits;
  DWORD reserved;
} STATSTG;

enum tagSTATFLAG {
  STATFLAG_DEFAULT = 0,
  STATFLAG_NONAME = 1,
  STATFLAG_NOOPEN = 2
};

//===--------------------- UUID Related Macros ----------------------------===//

// The following macros are defined to facilitate the lack of 'uuid' on Linux.

constexpr uint8_t nybble_from_hex(char c) {
  return ((c >= '0' && c <= '9')
              ? (c - '0')
              : ((c >= 'a' && c <= 'f')
                     ? (c - 'a' + 10)
                     : ((c >= 'A' && c <= 'F') ? (c - 'A' + 10)
                                               : /* Should be an error */ -1)));
}

constexpr uint8_t byte_from_hex(char c1, char c2) {
  return nybble_from_hex(c1) << 4 | nybble_from_hex(c2);
}

constexpr uint8_t byte_from_hexstr(const char str[2]) {
  return nybble_from_hex(str[0]) << 4 | nybble_from_hex(str[1]);
}

constexpr GUID guid_from_string(const char str[37]) {
  return GUID{static_cast<uint32_t>(byte_from_hexstr(str)) << 24 |
                  static_cast<uint32_t>(byte_from_hexstr(str + 2)) << 16 |
                  static_cast<uint32_t>(byte_from_hexstr(str + 4)) << 8 |
                  byte_from_hexstr(str + 6),
              static_cast<uint16_t>(
                  static_cast<uint16_t>(byte_from_hexstr(str + 9)) << 8 |
                  byte_from_hexstr(str + 11)),
              static_cast<uint16_t>(
                  static_cast<uint16_t>(byte_from_hexstr(str + 14)) << 8 |
                  byte_from_hexstr(str + 16)),
              {byte_from_hexstr(str + 19), byte_from_hexstr(str + 21),
               byte_from_hexstr(str + 24), byte_from_hexstr(str + 26),
               byte_from_hexstr(str + 28), byte_from_hexstr(str + 30),
               byte_from_hexstr(str + 32), byte_from_hexstr(str + 34)}};
}

#ifdef __EMULATE_UUID

template <typename interfce> inline GUID __emulated_uuidof();

#define CROSS_PLATFORM_UUIDOF(interfce, spec)                                  \
  struct interfce;                                                             \
  template <> inline GUID __emulated_uuidof<interfce>() {                      \
    static const IID _IID = guid_from_string(spec);                            \
    return _IID;                                                               \
  }                                                                            \
  extern "C++" {                                                               \
  template <> inline const GUID &__wsl_stub_uuidof<interfce>() {               \
    static const IID _IID = guid_from_string(spec);                            \
    return _IID;                                                               \
  }                                                                            \
  template <> inline const GUID &__wsl_stub_uuidof<interfce *>() {             \
    return __wsl_stub_uuidof<interfce>();                                      \
  }                                                                            \
  }

#else // __EMULATE_UUID

#ifndef _WIN32
#define CROSS_PLATFORM_UUIDOF(interfce, spec)                                  \
  struct __declspec(uuid(spec)) interfce;                                      \
  extern "C++" {                                                               \
  template <> inline const GUID &__wsl_stub_uuidof<interfce>() {               \
    static const IID _IID = guid_from_string(spec);                            \
    return _IID;                                                               \
  }                                                                            \
  template <> inline const GUID &__wsl_stub_uuidof<interfce *>() {             \
    return __wsl_stub_uuidof<interfce>();                                      \
  }                                                                            \
  }
#elif !defined(CROSS_PLATFORM_UUIDOF)
// Warning: This macro exists in dxcapi.h as well
#define CROSS_PLATFORM_UUIDOF(interfce, spec)                                  \
  struct __declspec(uuid(spec)) interfce;
#endif

#endif // __EMULATE_UUID

//===--------------------- COM Interfaces ---------------------------------===//

CROSS_PLATFORM_UUIDOF(INoMarshal, "ECC8691B-C1DB-4DC0-855E-65F6C551AF49")
struct INoMarshal : public IUnknown {};

CROSS_PLATFORM_UUIDOF(IMalloc, "00000002-0000-0000-C000-000000000046")
struct IMalloc : public IUnknown {
  virtual void *Alloc(SIZE_T size) = 0;
  virtual void *Realloc(void *ptr, SIZE_T size) = 0;
  virtual void Free(void *ptr) = 0;
  virtual SIZE_T GetSize(void *pv) = 0;
  virtual int DidAlloc(void *pv) = 0;
  virtual void HeapMinimize(void) = 0;
};

CROSS_PLATFORM_UUIDOF(ISequentialStream, "0C733A30-2A1C-11CE-ADE5-00AA0044773D")
struct ISequentialStream : public IUnknown {
  virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead) = 0;
  virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten) = 0;
};

CROSS_PLATFORM_UUIDOF(IStream, "0000000c-0000-0000-C000-000000000046")
struct IStream : public ISequentialStream {
  virtual HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin,
                       ULARGE_INTEGER *plibNewPosition) = 0;
  virtual HRESULT SetSize(ULARGE_INTEGER libNewSize) = 0;
  virtual HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb,
                         ULARGE_INTEGER *pcbRead,
                         ULARGE_INTEGER *pcbWritten) = 0;

  virtual HRESULT Commit(DWORD grfCommitFlags) = 0;

  virtual HRESULT Revert(void) = 0;

  virtual HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
                             DWORD dwLockType) = 0;

  virtual HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
                               DWORD dwLockType) = 0;

  virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag) = 0;

  virtual HRESULT Clone(IStream **ppstm) = 0;
};

// These don't need stub implementations as they come from the DirectX Headers
// They still need the __uuidof() though
CROSS_PLATFORM_UUIDOF(ID3D12LibraryReflection,
                      "8E349D19-54DB-4A56-9DC9-119D87BDB804")
CROSS_PLATFORM_UUIDOF(ID3D12LibraryReflection1,
                      "07692220-8721-4514-BFFE-969BB33196B6")
CROSS_PLATFORM_UUIDOF(ID3D12ShaderReflection,
                      "5A58797D-A72C-478D-8BA2-EFC6B0EFE88E")
CROSS_PLATFORM_UUIDOF(ID3D12ShaderReflection1,
                      "9C886958-23F1-4126-B294-FCF3DA13E7C6")

//===--------------------- COM Pointer Types ------------------------------===//

class CAllocator {
public:
  static void *Reallocate(void *p, size_t nBytes) throw();
  static void *Allocate(size_t nBytes) throw();
  static void Free(void *p) throw();
};

template <class T> class CComPtrBase {
protected:
  CComPtrBase() throw() { p = nullptr; }
  CComPtrBase(T *lp) throw() {
    p = lp;
    if (p != nullptr)
      p->AddRef();
  }
  void Swap(CComPtrBase &other) {
    T *pTemp = p;
    p = other.p;
    other.p = pTemp;
  }

public:
  ~CComPtrBase() throw() {
    if (p) {
      p->Release();
      p = nullptr;
    }
  }
  operator T *() const throw() { return p; }
  T &operator*() const { return *p; }
  T *operator->() const { return p; }
  T **operator&() throw() {
    assert(p == nullptr);
    return &p;
  }
  bool operator!() const throw() { return (p == nullptr); }
  bool operator<(T *pT) const throw() { return p < pT; }
  bool operator!=(T *pT) const { return !operator==(pT); }
  bool operator==(T *pT) const throw() { return p == pT; }

  // Release the interface and set to nullptr
  void Release() throw() {
    T *pTemp = p;
    if (pTemp) {
      p = nullptr;
      pTemp->Release();
    }
  }

  // Attach to an existing interface (does not AddRef)
  void Attach(T *p2) throw() {
    if (p) {
      ULONG ref = p->Release();
      (void)(ref);
      // Attaching to the same object only works if duplicate references are
      // being coalesced.  Otherwise re-attaching will cause the pointer to be
      // released and may cause a crash on a subsequent dereference.
      assert(ref != 0 || p2 != p);
    }
    p = p2;
  }

  // Detach the interface (does not Release)
  T *Detach() throw() {
    T *pt = p;
    p = nullptr;
    return pt;
  }

  HRESULT CopyTo(T **ppT) throw() {
    assert(ppT != nullptr);
    if (ppT == nullptr)
      return E_POINTER;
    *ppT = p;
    if (p)
      p->AddRef();
    return S_OK;
  }

  template <class Q> HRESULT QueryInterface(Q **pp) const throw() {
    assert(pp != nullptr);
    return p->QueryInterface(__uuidof(Q), (void **)pp);
  }

  T *p;
};

template <class T> class CComPtr : public CComPtrBase<T> {
public:
  CComPtr() throw() {}
  CComPtr(T *lp) throw() : CComPtrBase<T>(lp) {}
  CComPtr(const CComPtr<T> &lp) throw() : CComPtrBase<T>(lp.p) {}
  T *operator=(T *lp) throw() {
    if (*this != lp) {
      CComPtr(lp).Swap(*this);
    }
    return *this;
  }

  inline bool IsEqualObject(IUnknown *pOther) throw() {
    if (this->p == nullptr && pOther == nullptr)
      return true; // They are both NULL objects

    if (this->p == nullptr || pOther == nullptr)
      return false; // One is NULL the other is not

    CComPtr<IUnknown> punk1;
    CComPtr<IUnknown> punk2;
    this->p->QueryInterface(__uuidof(IUnknown), (void **)&punk1);
    pOther->QueryInterface(__uuidof(IUnknown), (void **)&punk2);
    return punk1 == punk2;
  }

  void ComPtrAssign(IUnknown **pp, IUnknown *lp, REFIID riid) {
    IUnknown *pTemp = *pp; // takes ownership
    if (lp == nullptr || FAILED(lp->QueryInterface(riid, (void **)pp)))
      *pp = nullptr;
    if (pTemp)
      pTemp->Release();
  }

  template <typename Q> T *operator=(const CComPtr<Q> &lp) throw() {
    if (!this->IsEqualObject(lp)) {
      ComPtrAssign((IUnknown **)&this->p, lp, __uuidof(T));
    }
    return *this;
  }

  // NOTE: This conversion constructor is not part of the official CComPtr spec;
  // however, it is needed to convert CComPtr<Q> to CComPtr<T> where T derives
  // from Q on Clang. MSVC compiles this conversion as first a call to
  // CComPtr<Q>::operator T*, followed by CComPtr<T>(T*), but Clang fails to
  // compile with error: no viable conversion from 'CComPtr<Q>' to 'CComPtr<T>'.
  template <typename Q>
  CComPtr(const CComPtr<Q> &lp) throw() : CComPtrBase<T>(lp.p) {}

  T *operator=(const CComPtr<T> &lp) throw() {
    if (*this != lp) {
      CComPtr(lp).Swap(*this);
    }
    return *this;
  }

  CComPtr(CComPtr<T> &&lp) throw() : CComPtrBase<T>() { lp.Swap(*this); }

  T *operator=(CComPtr<T> &&lp) throw() {
    if (*this != lp) {
      CComPtr(static_cast<CComPtr &&>(lp)).Swap(*this);
    }
    return *this;
  }
};

template <class T> class CSimpleArray : public std::vector<T> {
public:
  bool Add(const T &t) {
    this->push_back(t);
    return true;
  }
  int GetSize() { return this->size(); }
  T *GetData() { return this->data(); }
  void RemoveAll() { this->clear(); }
};

template <class T, class Allocator = CAllocator> class CHeapPtrBase {
protected:
  CHeapPtrBase() throw() : m_pData(NULL) {}
  CHeapPtrBase(CHeapPtrBase<T, Allocator> &p) throw() {
    m_pData = p.Detach(); // Transfer ownership
  }
  explicit CHeapPtrBase(T *pData) throw() : m_pData(pData) {}

public:
  ~CHeapPtrBase() throw() { Free(); }

protected:
  CHeapPtrBase<T, Allocator> &operator=(CHeapPtrBase<T, Allocator> &p) throw() {
    if (m_pData != p.m_pData)
      Attach(p.Detach()); // Transfer ownership
    return *this;
  }

public:
  operator T *() const throw() { return m_pData; }
  T *operator->() const throw() {
    assert(m_pData != NULL);
    return m_pData;
  }

  T **operator&() throw() {
    assert(m_pData == NULL);
    return &m_pData;
  }

  // Allocate a buffer with the given number of bytes
  bool AllocateBytes(size_t nBytes) throw() {
    assert(m_pData == NULL);
    m_pData = static_cast<T *>(Allocator::Allocate(nBytes * sizeof(char)));
    if (m_pData == NULL)
      return false;

    return true;
  }

  // Attach to an existing pointer (takes ownership)
  void Attach(T *pData) throw() {
    Allocator::Free(m_pData);
    m_pData = pData;
  }

  // Detach the pointer (releases ownership)
  T *Detach() throw() {
    T *pTemp = m_pData;
    m_pData = NULL;
    return pTemp;
  }

  // Free the memory pointed to, and set the pointer to NULL
  void Free() throw() {
    Allocator::Free(m_pData);
    m_pData = NULL;
  }

  // Reallocate the buffer to hold a given number of bytes
  bool ReallocateBytes(size_t nBytes) throw() {
    T *pNew;
    pNew =
        static_cast<T *>(Allocator::Reallocate(m_pData, nBytes * sizeof(char)));
    if (pNew == NULL)
      return false;
    m_pData = pNew;

    return true;
  }

public:
  T *m_pData;
};

template <typename T, class Allocator = CAllocator>
class CHeapPtr : public CHeapPtrBase<T, Allocator> {
public:
  CHeapPtr() throw() {}
  CHeapPtr(CHeapPtr<T, Allocator> &p) throw() : CHeapPtrBase<T, Allocator>(p) {}
  explicit CHeapPtr(T *p) throw() : CHeapPtrBase<T, Allocator>(p) {}
  CHeapPtr<T> &operator=(CHeapPtr<T, Allocator> &p) throw() {
    CHeapPtrBase<T, Allocator>::operator=(p);
    return *this;
  }

  // Allocate a buffer with the given number of elements
  bool Allocate(size_t nElements = 1) throw() {
    size_t nBytes = nElements * sizeof(T);
    return this->AllocateBytes(nBytes);
  }

  // Reallocate the buffer to hold a given number of elements
  bool Reallocate(size_t nElements) throw() {
    size_t nBytes = nElements * sizeof(T);
    return this->ReallocateBytes(nBytes);
  }
};

#define CComHeapPtr CHeapPtr

//===--------------------------- BSTR Allocation --------------------------===//

void SysFreeString(BSTR bstrString);
// Allocate string with length prefix
BSTR SysAllocStringLen(const OLECHAR *strIn, UINT ui);

//===--------------------------- BSTR Length ------------------------------===//
unsigned int SysStringLen(const BSTR bstrString);

//===--------------------- UTF-8 Related Types ----------------------------===//

// Code Page
#define CP_ACP 0
#define CP_UTF8 65001 // UTF-8 translation.

// RAII style mechanism for setting/unsetting a locale for the specified Windows
// codepage
class ScopedLocale {
  const char *m_prevLocale;

public:
  explicit ScopedLocale(uint32_t codePage)
      : m_prevLocale(setlocale(LC_ALL, nullptr)) {
    assert((codePage == CP_UTF8) &&
           "Support for Linux only handles UTF8 code pages");
    setlocale(LC_ALL, "en_US.UTF-8");
  }
  ~ScopedLocale() {
    if (m_prevLocale != nullptr) {
      setlocale(LC_ALL, m_prevLocale);
    }
  }
};

// The t_nBufferLength parameter is part of the published interface, but not
// used here.
template <int t_nBufferLength = 128> class CW2AEX {
public:
  CW2AEX(LPCWSTR psz) {
    ScopedLocale locale(CP_UTF8);

    if (!psz) {
      m_psz = NULL;
      return;
    }

    int len = (wcslen(psz) + 1) * 4;
    m_psz = new char[len];
    std::wcstombs(m_psz, psz, len);
  }

  ~CW2AEX() { delete[] m_psz; }

  operator LPSTR() const { return m_psz; }

  char *m_psz;
};
typedef CW2AEX<> CW2A;

// The t_nBufferLength parameter is part of the published interface, but not
// used here.
template <int t_nBufferLength = 128> class CA2WEX {
public:
  CA2WEX(LPCSTR psz) {
    ScopedLocale locale(CP_UTF8);

    if (!psz) {
      m_psz = NULL;
      return;
    }

    int len = strlen(psz) + 1;
    m_psz = new wchar_t[len];
    std::mbstowcs(m_psz, psz, len);
  }

  ~CA2WEX() { delete[] m_psz; }

  operator LPWSTR() const { return m_psz; }

  wchar_t *m_psz;
};

typedef CA2WEX<> CA2W;

//===--------- File IO Related Types ----------------===//

class CHandle {
public:
  CHandle(HANDLE h);
  ~CHandle();
  operator HANDLE() const throw();

private:
  HANDLE m_h;
};

/////////////////////////////////////////////////////////////////////////////
// CComBSTR

class CComBSTR {
public:
  BSTR m_str;
  CComBSTR() : m_str(nullptr){};
  CComBSTR(int nSize, LPCWSTR sz);
  ~CComBSTR() throw() { SysFreeString(m_str); }
  unsigned int Length() const throw() { return SysStringLen(m_str); }
  operator BSTR() const throw() { return m_str; }

  bool operator==(const CComBSTR &bstrSrc) const throw();

  BSTR *operator&() throw() { return &m_str; }

  BSTR Detach() throw() {
    BSTR s = m_str;
    m_str = NULL;
    return s;
  }

  void Empty() throw() {
    SysFreeString(m_str);
    m_str = NULL;
  }
};

//===--------- Convert argv to wchar ----------------===//
class WArgV {
  std::vector<std::wstring> WStringVector;
  std::vector<const wchar_t *> WCharPtrVector;

public:
  WArgV(int argc, const char **argv);
  const wchar_t **argv() { return WCharPtrVector.data(); }
};

#endif // __cplusplus

#endif // _WIN32

#endif // LLVM_SUPPORT_WIN_ADAPTER_H
