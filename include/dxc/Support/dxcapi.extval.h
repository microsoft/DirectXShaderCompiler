#include "dxc/Support/dxcapi.use.h"
#include <string>

namespace dxc {
class DxcDllExtValidationSupport {
  // DxcompilerSupport manages the
  // lifetime of dxcompiler.dll, while DxilExtValSupport
  // manages the lifetime of dxil.dll
  dxc::DxcDllSupport DxcompilerSupport;
  dxc::DxcDllSupport DxilExtValSupport;

  std::string DxilDllPath;
  HRESULT InitializeInternal(LPCSTR dllName, LPCSTR fnName);

public:
  std::string GetDxilDllPath() { return DxilDllPath; }
  bool DxilDllFailedToLoad() {
    return !DxilDllPath.empty() && !DxilExtValSupport.IsEnabled();
  }

  void Cleanup() {
    DxilExtValSupport.Cleanup();
    DxcompilerSupport.Cleanup();
  }

  HMODULE Detach() {
    // Can't Detach and return a handle for DxilSupport. Cleanup() instead.
    DxilExtValSupport.Cleanup();
    return DxcompilerSupport.Detach();
  }

  HRESULT CreateInstance(REFCLSID clsid, REFIID riid, IUnknown **pResult);
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                          IUnknown **pResult);

  HRESULT Initialize() {
    return InitializeInternal(kDxCompilerLib, "DxcCreateInstance");
  }

  bool IsEnabled() const { return DxcompilerSupport.IsEnabled(); }
};
} // namespace dxc