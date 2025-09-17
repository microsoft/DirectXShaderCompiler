#include "dxc/Support/WinIncludes.h"
#include <dxc/Support/Global.h> // for hresult handling with DXC_FAILED
#include <filesystem>           // C++17 and later
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/dxcapi.extval.h"

std::vector<LPCWSTR> AddVDCompilationArg(UINT32 ArgCount, LPCWSTR *pArguments) {
  UINT32 newArgCount = ArgCount + 1;
  std::vector<LPCWSTR> newArgs;
  newArgs.reserve(newArgCount);

  // Copy existing args
  for (unsigned int i = 0; i < ArgCount; ++i) {
    newArgs.push_back(pArguments[i]);
  }

  // Add an extra argument
  newArgs.push_back(L"-Vd");
  return newArgs;
}

namespace dxc {

// The purpose of the ExternalValidationHelper* wrappers is
// to wrap the Compile call so that -Vd can be passed, disabling
// internal validation. Then, we can allow the caller to handle
// external validation. This wrapper is used when in an external
// validation scenario, so there is no point in also running
// the internal validator
class ExternalValidationHelper3 : public IDxcCompiler3 {
private:
  CComPtr<IDxcCompiler3> m_pCompiler3;
  CComPtr<IDxcValidator> m_pValidator;

public:
  HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer *pSource,
                                    LPCWSTR *pArguments, UINT32 argCount,
                                    IDxcIncludeHandler *pIncludeHandler,
                                    REFIID riid, LPVOID *ppResult) override {
    // First, add -Vd to the compilation args, to disable the
    // internal validator
    std::vector<LPCWSTR> newArgs = AddVDCompilationArg(argCount, pArguments);
    return m_pCompiler3->Compile(pSource, newArgs.data(), newArgs.size(),
                                 pIncludeHandler, riid, ppResult);
  }

  HRESULT STDMETHODCALLTYPE Disassemble(const DxcBuffer *pObject, REFIID riid,
                                        LPVOID *ppResult) override {
    return m_pCompiler3->Disassemble(pObject, riid, ppResult);
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    return m_pCompiler3->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return m_pCompiler3.p->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() override {
    return m_pCompiler3.p->Release();
  }

  HRESULT STDMETHODCALLTYPE Validate(IDxcBlob *pShader, UINT32 Flags,
                                     IDxcOperationResult **ppResult) {
    return m_pValidator->Validate(pShader, Flags, ppResult);
  }

  ExternalValidationHelper3(CComPtr<IDxcCompiler3> pCompiler,
                            CComPtr<IDxcValidator> pValidator) {
    m_pCompiler3 = pCompiler;
    m_pValidator = pValidator;
  }
};

class ExternalValidationHelper : public IDxcCompiler {
private:
  CComPtr<IDxcCompiler> m_pCompiler;
  CComPtr<IDxcValidator> m_pValidator;

public:
  HRESULT STDMETHODCALLTYPE Compile(IDxcBlob *pSource, LPCWSTR pSourceName,
                                    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
                                    LPCWSTR *pArguments, UINT32 argCount,
                                    const DxcDefine *pDefines,
                                    UINT32 defineCount,
                                    IDxcIncludeHandler *pIncludeHandler,
                                    IDxcOperationResult **ppResult) override {
    // First, add -Vd to the compilation args, to disable the
    // internal validator
    std::vector<LPCWSTR> newArgs = AddVDCompilationArg(argCount, pArguments);
    return m_pCompiler->Compile(
        pSource, pSourceName, pEntryPoint, pTargetProfile, newArgs.data(),
        newArgs.size(), pDefines, defineCount, pIncludeHandler, ppResult);
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

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    return m_pCompiler->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return m_pCompiler.p->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() override {
    return m_pCompiler.p->Release();
  }

  HRESULT STDMETHODCALLTYPE Validate(IDxcBlob *pShader, UINT32 Flags,
                                     IDxcOperationResult **ppResult) {
    return m_pValidator->Validate(pShader, Flags, ppResult);
  }

  ExternalValidationHelper(CComPtr<IDxcCompiler> pCompiler,
                           CComPtr<IDxcValidator> pValidator) {
    m_pCompiler = pCompiler;
    m_pValidator = pValidator;
  }
};

HRESULT DxcDllExtValidationLoader::CreateInstanceImpl(REFCLSID clsid,
                                                      REFIID riid,
                                                      IUnknown **pResult) {
  if (pResult == nullptr)
    return E_POINTER;

  *pResult = nullptr;

  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcCompiler) {

      // Create validator
      CComPtr<IDxcValidator> m_pValidator;
      HRESULT ValHR = DxilExtValSupport.CreateInstance<IDxcValidator>(
          CLSID_DxcValidator, &m_pValidator);
      if (FAILED(ValHR))
        return ValHR;

      // Wrap compiler + validator
      // The wrapper should be selected based on the target interface
      if (riid == __uuidof(IDxcCompiler3)) {
        // Create compiler
        CComPtr<IDxcCompiler3> m_pCompiler3;
        HRESULT hr = DxCompilerSupport.CreateInstance<IDxcCompiler3>(
            CLSID_DxcCompiler, &m_pCompiler3);
        if (FAILED(hr))
          return hr;
        ExternalValidationHelper3 *evh = new (std::nothrow)
            ExternalValidationHelper3(m_pCompiler3, m_pValidator);
        if (!evh)
          return E_OUTOFMEMORY;

        hr = evh->QueryInterface(riid, reinterpret_cast<void **>(pResult));
        evh->Release();
        return hr;
      } else if (riid == __uuidof(IDxcCompiler)) {
        // Create compiler
        CComPtr<IDxcCompiler> m_pCompiler;
        HRESULT hr = DxCompilerSupport.CreateInstance<IDxcCompiler>(
            CLSID_DxcCompiler, &m_pCompiler);
        if (FAILED(hr))
          return hr;
        ExternalValidationHelper *evh = new (std::nothrow)
            ExternalValidationHelper(m_pCompiler, m_pValidator);
        if (!evh)
          return E_OUTOFMEMORY;

        hr = evh->QueryInterface(riid, reinterpret_cast<void **>(pResult));
        evh->Release();
        return hr;
      }

    } else if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance<IDxcValidator>(
          clsid, reinterpret_cast<IDxcValidator **>(pResult));
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance(clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2Impl(IMalloc *pMalloc,
                                                       REFCLSID clsid,
                                                       REFIID riid,
                                                       IUnknown **pResult) {
  if (pResult == nullptr)
    return E_POINTER;

  *pResult = nullptr;
  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcCompiler) {

      // Create compiler
      CComPtr<IDxcCompiler3> m_pCompiler;
      HRESULT hr = DxCompilerSupport.CreateInstance2<IDxcCompiler3>(
          pMalloc, CLSID_DxcCompiler, &m_pCompiler);
      if (FAILED(hr))
        return hr;

      // Create validator
      CComPtr<IDxcValidator> m_pValidator;
      hr = DxilExtValSupport.CreateInstance2<IDxcValidator>(
          pMalloc, CLSID_DxcValidator, &m_pValidator);
      if (FAILED(hr))
        return hr;

      // Wrap compiler + validator
      ExternalValidationHelper3 *evh = new (std::nothrow)
          ExternalValidationHelper3(m_pCompiler, m_pValidator);
      if (!evh)
        return E_OUTOFMEMORY;

      hr = evh->QueryInterface(riid, reinterpret_cast<void **>(pResult));
      evh->Release();
      return hr;
    } else if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance2<IDxcValidator>(
          pMalloc, clsid, reinterpret_cast<IDxcValidator **>(pResult));
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
