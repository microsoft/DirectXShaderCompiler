#include "dxc/Support/WinIncludes.h"
#include <dxc/Support/Global.h> // for hresult handling with DXC_FAILED
#include <filesystem>           // C++17 and later
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/dxcapi.extval.h"

namespace dxc {

HRESULT DxcDllExtValidationSupport::CreateInstance(REFCLSID clsid, REFIID riid,
                                                   IUnknown **pResult) {
  if (DxilExtValSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilExtValSupport.CreateInstance(clsid, riid, pResult);

  return DxcompilerSupport.CreateInstance(clsid, riid, pResult);
}

HRESULT DxcDllExtValidationSupport::CreateInstance2(IMalloc *pMalloc,
                                                    REFCLSID clsid, REFIID riid,
                                                    IUnknown **pResult) {
  if (DxilExtValSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilExtValSupport.CreateInstance2(pMalloc, clsid, riid, pResult);

  return DxcompilerSupport.CreateInstance2(pMalloc, clsid, riid, pResult);
}

HRESULT DxcDllExtValidationSupport::InitializeInternal(LPCSTR dllName,
                                                       LPCSTR fnName) {
  // Load dxcompiler.dll
  HRESULT Result = DxcompilerSupport.InitializeForDll(dllName, fnName);
  // if dxcompiler.dll fails to load, return the failed HRESULT
  if (DXC_FAILED(Result)) {
    return Result;
  }

  // now handle external dxil.dll
  const char *EnvVarVal = std::getenv("DXC_DXIL_DLL_PATH");
  if (!EnvVarVal || std::string(EnvVarVal).empty()) {
    return S_OK;
  }

  std::string DllPathStr(EnvVarVal);
  DxilDllPath = DllPathStr;
  std::filesystem::path DllPath(DllPathStr);

  // Check if path is absolute and exists
  if (!DllPath.is_absolute() || !std::filesystem::exists(DllPath)) {
    return S_OK;
  }
  // code that calls this function is responsible for checking
  // to see if dxil.dll is successfully loaded.
  // the CheckDxilDLLLoaded function can determine whether there were any
  // problems loading dxil.dll or not
  DxilExtValSupport.InitializeForDll(DllPathStr.data(), fnName);

  return S_OK;
}

} // namespace dxc