
#ifndef _WIN32
#include "dxc/WinAdapter.h"
#endif

#include "dxc/Support/WinIncludes.h"
#include <dxc/Support/Global.h> // for hresult handling with DXC_FAILED
#include <filesystem>           // C++17 and later
#include <sstream>
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/dxcapi.extval.h"
#include "dxc/Support/microcom.h"
#include <iostream>

static std::vector<std::wstring> AddExtValCompilationArgs(UINT32 ArgCount,
                                                          LPCWSTR *pArguments,
                                                          UINT32 Major,
                                                          UINT32 Minor) {
  /* We need to add 3 args to the compiler automatically in the external
     validation case:
    1. -Vd, to disable internal validation. No need to run the internal
    validator when the external validator was requested to validate.
    2. -validator-version to write the validator version to the resulting
    module. Without this, the modern compiler will write its own version, which
    will be to0 new for older validators, and the validator will fail. However,
    if explicitly specified on the command line, we shouldn't overwrite it.
    3. -Od to remove optimization and have a reliable validation step,
    expecially with older validators.
  */
  std::vector<std::wstring> newArgs = {pArguments, pArguments + ArgCount};

  // Track whether the user already passed -validator-version
  bool hasValidatorVersion = false;
  for (size_t i = 0; i < newArgs.size(); ++i) {
    if (newArgs[i] == L"-validator-version") {
      // Either the value is in the same arg ("-validator-version=1.6")
      // or the next arg is the version ("-validator-version", "1.6").
      hasValidatorVersion = true;
      break;
    }
    if (newArgs[i].rfind(L"-validator-version", 0) == 0) {
      // Handles "-validator-version=1.6"
      hasValidatorVersion = true;
      break;
    }
  }

  // Add the extra arguments
  newArgs.push_back(L"-Vd");

  // Add -validator-version only if not provided already
  if (!hasValidatorVersion) {
    std::wstringstream ss;
    ss << L"-validator-version " << Major << L"." << Minor;
    newArgs.push_back(ss.str());
  }

  newArgs.push_back(L"-Od");

  return newArgs;
}

namespace dxc {

// The purpose of the ExternalValidationHelper wrapper is
// to wrap the Compile call so that -Vd can be passed, disabling
// internal validation. Then, we can allow the caller to handle
// external validation. This wrapper is used when in an external
// validation scenario, so there is no point in also running
// the internal validator. This wrapper wraps both the
// IDxcCompiler and IDxcCompiler3 interfaces.
class ExternalValidationHelper : public IDxcCompiler, public IDxcCompiler3 {
private:
  CComPtr<IDxcCompiler> m_pCompiler;
  CComPtr<IDxcCompiler3> m_pCompiler3;
  UINT32 ValidatorVersionMajor;
  UINT32 ValidatorVersionMinor;

public:
  ExternalValidationHelper(CComPtr<IDxcCompiler> compiler,
                           CComPtr<IDxcCompiler3> compiler3, UINT32 major,
                           UINT32 minor, IMalloc *pMalloc = nullptr)
      : m_pCompiler(compiler), m_pCompiler3(compiler3),
        ValidatorVersionMajor(major), ValidatorVersionMinor(minor),
        m_pMalloc(pMalloc) {}

  // IUnknown implementation
  DXC_MICROCOM_TM_REF_FIELDS()
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    if (!ppvObject)
      return E_POINTER;

    *ppvObject = nullptr;

    // Windows: use built-in __uuidof
    if (IsEqualIID(iid, __uuidof(IUnknown))) {
      AddRef();
      *ppvObject = static_cast<IDxcCompiler3 *>(this);
      return S_OK;
    }

    if (m_pCompiler && IsEqualIID(iid, __uuidof(IDxcCompiler))) {
      AddRef();
      *ppvObject = static_cast<IDxcCompiler *>(this);
      return S_OK;
    }

    if (m_pCompiler3 && IsEqualIID(iid, __uuidof(IDxcCompiler3))) {
      AddRef();
      *ppvObject = static_cast<IDxcCompiler3 *>(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  // IDxcCompiler implementation
  HRESULT STDMETHODCALLTYPE Compile(IDxcBlob *pSource, LPCWSTR pSourceName,
                                    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
                                    LPCWSTR *pArguments, UINT32 argCount,
                                    const DxcDefine *pDefines,
                                    UINT32 defineCount,
                                    IDxcIncludeHandler *pIncludeHandler,
                                    IDxcOperationResult **ppResult) override {
    // First, add the external validation compilation args
    std::vector<std::wstring> newArgs = AddExtValCompilationArgs(
        argCount, pArguments, ValidatorVersionMajor, ValidatorVersionMinor);

    // Now build an array of LPCWSTRs for the API call:
    std::vector<LPCWSTR> rawArgs;
    rawArgs.reserve(newArgs.size());
    for (auto &a : newArgs) {
      rawArgs.push_back(a.c_str());
    }

    return m_pCompiler->Compile(
        pSource, pSourceName, pEntryPoint, pTargetProfile, rawArgs.data(),
        rawArgs.size(), pDefines, defineCount, pIncludeHandler, ppResult);
  }

  HRESULT STDMETHODCALLTYPE
  Preprocess(IDxcBlob *pSource, LPCWSTR pSourceName, LPCWSTR *pArguments,
             UINT32 argCount, const DxcDefine *pDefines, UINT32 defineCount,
             IDxcIncludeHandler *pIncludeHandler,
             IDxcOperationResult **ppResult) override {
    return m_pCompiler->Preprocess(pSource, pSourceName, pArguments, argCount,
                                   pDefines, defineCount, pIncludeHandler,
                                   ppResult);
  }

  HRESULT STDMETHODCALLTYPE
  Disassemble(IDxcBlob *pSource, IDxcBlobEncoding **ppDisassembly) override {
    return m_pCompiler->Disassemble(pSource, ppDisassembly);
  }

  // IDxcCompiler3 implementation
  HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer *pSource,
                                    LPCWSTR *pArguments, UINT32 argCount,
                                    IDxcIncludeHandler *pIncludeHandler,
                                    REFIID riid, LPVOID *ppResult) override {
    // First, add -Vd to the compilation args, to disable the
    // internal validator
    std::vector<std::wstring> newArgs = AddExtValCompilationArgs(
        argCount, pArguments, ValidatorVersionMajor, ValidatorVersionMinor);

    // Build raw LPCWSTR* array from owned wstrings
    std::vector<LPCWSTR> argPtrs;
    argPtrs.reserve(newArgs.size());
    for (auto &s : newArgs) {
      argPtrs.push_back(s.c_str());
    }

    return m_pCompiler3->Compile(pSource, argPtrs.data(),
                                 static_cast<UINT32>(argPtrs.size()),
                                 pIncludeHandler, riid, ppResult);
  }

  HRESULT STDMETHODCALLTYPE Disassemble(const DxcBuffer *pObject, REFIID riid,
                                        LPVOID *ppResult) override {
    return m_pCompiler3->Disassemble(pObject, riid, ppResult);
  }
};

static ExternalValidationHelper *Alloc(IMalloc *pMalloc,
                                       CComPtr<IDxcCompiler> compiler,
                                       CComPtr<IDxcCompiler3> compiler3,
                                       UINT32 major, UINT32 minor) {
  void *P = pMalloc->Alloc(sizeof(ExternalValidationHelper));
  try {
    if (P)
      return new (P)
          ExternalValidationHelper(compiler, compiler3, major, minor, pMalloc);
  } catch (...) {
    pMalloc->Free(P);
    throw;
  }
  return nullptr;
}

HRESULT DxcDllExtValidationLoader::CreateInstanceImpl(REFCLSID clsid,
                                                      REFIID riid,
                                                      IUnknown **pResult) {
  if (!pResult)
    return E_POINTER;

  *pResult = nullptr;

  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance<IDxcValidator>(
          clsid, reinterpret_cast<IDxcValidator **>(pResult));
    }
    if (clsid == CLSID_DxcCompiler) {

      CComPtr<IDxcCompiler> DxcCompiler;
      CComPtr<IDxcCompiler3> DxcCompiler3;
      CComPtr<IDxcValidator> DxcValidator;
      UINT32 validatorMajor, validatorMinor = 0;

      // first, get the validator version so that the wrapper knows
      // which -validator-version parameter to pass to the compiler's
      // compile function
      if (SUCCEEDED(DxilExtValSupport.CreateInstance<IDxcValidator>(
              CLSID_DxcValidator, &DxcValidator))) {

        CComPtr<IDxcVersionInfo> verInfo;
        if (SUCCEEDED(DxcValidator->QueryInterface(IID_PPV_ARGS(&verInfo)))) {
          verInfo->GetVersion(&validatorMajor, &validatorMinor);
        }
      }

      // Create compiler
      HRESULT hr = S_OK;
      if (riid == __uuidof(IDxcCompiler))
        hr = DxCompilerSupport.CreateInstance<IDxcCompiler>(CLSID_DxcCompiler,
                                                            &DxcCompiler);
      else if (riid == __uuidof(IDxcCompiler3))
        hr = DxCompilerSupport.CreateInstance<IDxcCompiler3>(CLSID_DxcCompiler,
                                                             &DxcCompiler3);
      else {
        // TODO: IDxcCompiler2 not implemented yet, but should
        // error for now
        hr = E_UNEXPECTED;
      }

      if (FAILED(hr))
        return hr;

      CComPtr<IMalloc> pDefaultMalloc = nullptr;
      hr = DxcCoGetMalloc(1 /*MEMCTX_TASK*/, &pDefaultMalloc);
      if (FAILED(hr))
        return hr;
      ExternalValidationHelper *evh =
          Alloc(pDefaultMalloc, DxcCompiler, DxcCompiler3, validatorMajor,
                validatorMinor);
      pDefaultMalloc.p->Release(); // Alloc holds a copy in m_pMalloc
      if (!evh)
        return E_OUTOFMEMORY;

      return evh->QueryInterface(riid, reinterpret_cast<void **>(pResult));
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance(clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2Impl(IMalloc *pMalloc,
                                                       REFCLSID clsid,
                                                       REFIID riid,
                                                       IUnknown **pResult) {
  if (!pResult)
    return E_POINTER;

  *pResult = nullptr;
  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance2<IDxcValidator>(
          pMalloc, clsid, reinterpret_cast<IDxcValidator **>(pResult));
    }
    if (clsid == CLSID_DxcCompiler) {
      CComPtr<IDxcCompiler> DxcCompiler;
      CComPtr<IDxcCompiler3> DxcCompiler3;
      CComPtr<IDxcValidator> DxcValidator;
      UINT32 validatorMajor, validatorMinor = 0;

      // first, get the validator version so that the wrapper knows
      // which -validator-version parameter to pass to the compiler's
      // compile function

      if (SUCCEEDED(DxilExtValSupport.CreateInstance<IDxcValidator>(
              CLSID_DxcValidator, &DxcValidator))) {

        CComPtr<IDxcVersionInfo> verInfo;
        if (SUCCEEDED(DxcValidator->QueryInterface(IID_PPV_ARGS(&verInfo)))) {
          verInfo->GetVersion(&validatorMajor, &validatorMinor);
        }
      }

      // Create compiler
      HRESULT hr = (riid == __uuidof(IDxcCompiler))
                       ? DxCompilerSupport.CreateInstance2<IDxcCompiler>(
                             pMalloc, CLSID_DxcCompiler, &DxcCompiler)
                       : DxCompilerSupport.CreateInstance2<IDxcCompiler3>(
                             pMalloc, CLSID_DxcCompiler, &DxcCompiler3);

      // Allocate properly with TM allocator
      ExternalValidationHelper *evh = Alloc(pMalloc, DxcCompiler, DxcCompiler3,
                                            validatorMajor, validatorMinor);
      if (!evh)
        return E_OUTOFMEMORY;

      // QueryInterface to the requested riid
      hr = evh->QueryInterface(riid, reinterpret_cast<void **>(pResult));
      return hr;
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance2(pMalloc, clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::Initialize(llvm::raw_string_ostream &log) {
  // Load dxcompiler.dll
  HRESULT Result =
      DxCompilerSupport.InitializeForDll(kDxCompilerLib, "DxcCreateInstance");
  // if dxcompiler.dll fails to load, return the failed HRESULT
  if (DXC_FAILED(Result)) {
    log << "dxcompiler.dll failed to load";
    return Result;
  }

  // now handle external dxil.dll
  const char *EnvVarVal = std::getenv("DXC_DXIL_DLL_PATH");
  if (!EnvVarVal || std::string(EnvVarVal).empty()) {
    // no need to emit anything if external validation isn't wanted
    return S_OK;
  }

  DxilDllPath = std::string(EnvVarVal);
  std::filesystem::path DllPath(DxilDllPath);

  // Check if path is absolute and exists
  if (!DllPath.is_absolute() || !std::filesystem::exists(DllPath)) {
    log << "dxil.dll path " << DxilDllPath << " could not be found";
    return E_INVALIDARG;
  }

  log << "Loading external dxil.dll from " << DxilDllPath;
  Result = DxilExtValSupport.InitializeForDll(DxilDllPath.c_str(),
                                              "DxcCreateInstance");
  if (DXC_FAILED(Result)) {
    log << "dxil.dll failed to load";
    return Result;
  }

  return Result;
}

} // namespace dxc
