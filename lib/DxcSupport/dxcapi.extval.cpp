#include "dxc/Support/WinIncludes.h"
#include <dxc/Support/Global.h> // for hresult handling with DXC_FAILED
#include <filesystem>           // C++17 and later
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/dxcapi.extval.h"

namespace dxc {

HRESULT DxcDllExtValidationLoader::CreateInstance(REFCLSID clsid, REFIID riid,
                                                  IUnknown **pResult) {
  if (DxilExtValSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilExtValSupport.CreateInstance(clsid, riid, pResult);

  return DxCompilerSupport.CreateInstance(clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2(IMalloc *pMalloc,
                                                   REFCLSID clsid, REFIID riid,
                                                   IUnknown **pResult) {
  if (DxilExtValSupport.IsEnabled() && clsid == CLSID_DxcValidator)
    return DxilExtValSupport.CreateInstance2(pMalloc, clsid, riid, pResult);

  return DxCompilerSupport.CreateInstance2(pMalloc, clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::InitializeInternal(LPCSTR fnName) {
  // Load dxcompiler.dll
  HRESULT Result = DxCompilerSupport.OverrideDll(kDxCompilerLib, fnName);
  // if dxcompiler.dll fails to load, return the failed HRESULT
  if (DXC_FAILED(Result)) {
    return Result;
  }

  // now handle external dxil.dll
  const char *EnvVarVal = std::getenv("DXC_DXIL_DLL_PATH");
  if (!EnvVarVal || std::string(EnvVarVal).empty()) {
    return S_OK;
  }

  DxilDllPath = std::string(EnvVarVal);
  std::filesystem::path DllPath(DxilDllPath);

  // Check if path is absolute and exists
  if (!DllPath.is_absolute() || !std::filesystem::exists(DllPath)) {
    return E_INVALIDARG;
  }

  return DxilExtValSupport.OverrideDll(DxilDllPath.c_str(), fnName);
}

bool DxcDllExtValidationLoader::GetCreateInstanceProcs(
    DxcCreateInstanceProc *pCreateFn,
    DxcCreateInstance2Proc *pCreateFn2) const {
  if (pCreateFn == nullptr || pCreateFn2 == nullptr || m_createFn == nullptr)
    return false;
  *pCreateFn = m_createFn;
  *pCreateFn2 = m_createFn2;
  return true;
}

} // namespace dxc