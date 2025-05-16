#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.h"

class DxcDllExtValidationSupport : public dxc::DxcDllSupport {
  // DxcDllExtValidationSupport manages the
  // lifetime of dxcompiler.dll, while the member, m_DxilSupport,
  // manages the lifetime of dxil.dll
  dxc::DxcDllSupport m_DxilSupport;

  std::string DxilDllPath;
  // override DxcDllSupport's implementation of InitializeInternal,
  // adding the environment variable value check for a path to a dxil.dll

  std::string GetDxilDLLPathExt() { return DxilDllPath; }
  bool DxilDllSuccessfullyLoaded() {
    return !DxilDllPath.empty() && !m_DxilSupport.IsEnabled();
  }

  HRESULT InitializeInternal(LPCSTR dllName, LPCSTR fnName);
};