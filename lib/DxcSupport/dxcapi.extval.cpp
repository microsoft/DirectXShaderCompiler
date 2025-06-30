#include "dxc/Support/WinIncludes.h"
#include <dxc/Support/Global.h> // for hresult handling with DXC_FAILED
#include <filesystem>           // C++17 and later
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/dxcapi.extval.h"

HRESULT DxcDllExtValidationSupport::InitializeInternal(LPCSTR dllName,
                                                       LPCSTR fnName) {
  // Load dxcompiler.dll
  HRESULT Result = DxcDllSupport::InitializeInternal(dllName, fnName);
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
  DxilSupport.InitializeForDll(DllPathStr.data(), fnName);

  return S_OK;
}

HRESULT DxcDllExtValidationSupport::CreateInstance(REFCLSID clsid, REFIID riid,
                                                   IUnknown **pResult) {
  if (DxilSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilSupport.CreateInstance(clsid, riid, pResult);

  if (pResult == nullptr)
    return E_POINTER;
  if (m_dll == nullptr)
    return E_FAIL;
  HRESULT hr = m_createFn(clsid, riid, (LPVOID *)pResult);
  return hr;
}

HRESULT DxcDllExtValidationSupport::CreateInstance2(IMalloc *pMalloc,
                                                    REFCLSID clsid, REFIID riid,
                                                    IUnknown **pResult) {
  if (DxilSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilSupport.CreateInstance2(pMalloc, clsid, riid, pResult);

  if (pResult == nullptr)
    return E_POINTER;
  if (m_dll == nullptr)
    return E_FAIL;
  if (m_createFn2 == nullptr)
    return E_FAIL;
  HRESULT hr = m_createFn2(pMalloc, clsid, riid, (LPVOID *)pResult);
  return hr;
}
