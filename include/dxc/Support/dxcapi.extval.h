#include "dxc/Support/dxcapi.use.h"
#include <string>

class DxcDllExtValidationSupport : public dxc::DxcDllSupport {
  // DxcDllExtValidationSupport manages the
  // lifetime of dxcompiler.dll, while the member, DxilSupport,
  // manages the lifetime of dxil.dll
protected:
  dxc::DxcDllSupport DxilSupport;

  std::string DxilDllPath;

  // override DxcDllSupport's implementation of InitializeInternal,
  // adding the environment variable value check for a path to a dxil.dll
  HRESULT InitializeInternal(LPCSTR dllName, LPCSTR fnName) override;

public:
  std::string GetDxilDllPath() { return DxilDllPath; }
  bool DxilDllFailedToLoad() {
    return !DxilDllPath.empty() && !DxilSupport.IsEnabled();
  }

  void Cleanup() override {
    DxilSupport.Cleanup();
    DxcDllSupport::Cleanup();
  }

  HMODULE Detach() override {
    // Can't Detach and return a handle for DxilSupport. Cleanup() instead.
    DxilSupport.Cleanup();
    return DxcDllSupport::Detach();
  }

  HRESULT CreateInstance(REFCLSID clsid, REFIID riid,
                         IUnknown **pResult) override;
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                          IUnknown **pResult) override;
};
