#include "dxc/Support/WinIncludes.h"
#include <dxc/Support/Global.h> // for hresult handling with DXC_FAILED
#include <filesystem>           // C++17 and later
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/dxcapi.extval.h"

namespace dxc {

HRESULT STDMETHODCALLTYPE ExternalValidationHelper::Compile(
    _In_ const DxcBuffer *pSource, ///< Source text to compile.
    _In_opt_count_(argCount)
        LPCWSTR *pArguments, ///< Array of pointers to arguments.
    _In_ UINT32 argCount,    ///< Number of arguments.
    _In_opt_ IDxcIncludeHandler
        *pIncludeHandler, ///< user-provided interface to handle include
    ///< directives (optional).
    _In_ REFIID riid,      ///< Interface ID for the result.
    _Out_ LPVOID *ppResult ///< IDxcResult: status, buffer, and errors.
) {
  // First, add -Vd to the compilation args
  UINT32 newArgCount = argCount + 1;
  std::vector<LPCWSTR> newArgs;
  newArgs.reserve(newArgCount);

  // Copy existing args
  for (unsigned int i = 0; i < argCount; ++i) {
    newArgs.push_back(pArguments[i]);
  }

  // Add an extra argument
  newArgs.push_back(L"-Vd");

  return m_pCompiler->Compile(pSource, newArgs.data(), newArgCount,
                              pIncludeHandler, riid, ppResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstanceImpl(REFCLSID clsid,
                                                      REFIID riid,
                                                      IUnknown **pResult) {
  if (DxilExtValSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilExtValSupport.CreateInstance(clsid, riid, pResult);

  return DxCompilerSupport.CreateInstance(clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2Impl(IMalloc *pMalloc,
                                                       REFCLSID clsid,
                                                       REFIID riid,
                                                       IUnknown **pResult) {
  if (DxilExtValSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilExtValSupport.CreateInstance2(pMalloc, clsid, riid, pResult);

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
  CComPtr<IDxcCompiler3> m_pCompiler;
  CComPtr<IDxcValidator> m_pValidator;
  DxCompilerSupport.CreateInstance(CLSID_DxcCompiler, &m_pCompiler);
  DxilExtValSupport.CreateInstance(CLSID_DxcValidator, &m_pValidator);
  ValWrapperObj = ExternalValidationHelper(m_pCompiler, m_pValidator);
  return Result;
}

} // namespace dxc
