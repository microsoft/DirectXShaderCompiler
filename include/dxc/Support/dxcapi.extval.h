#include "dxc/Support/dxcapi.use.h"
#include <string>

class DxcDllExtValidationSupport : public dxc::DxcDllSupport {
  // DxcDllExtValidationSupport manages the
  // lifetime of dxcompiler.dll, while the member, m_DxilSupport,
  // manages the lifetime of dxil.dll
  protected:
  dxc::DxcDllSupport DxilSupport;

  std::string DxilDllPath;

  HRESULT InitializeInternal(LPCSTR dllName, LPCSTR fnName) override;

  public:
  // override DxcDllSupport's implementation of InitializeInternal,
  // adding the environment variable value check for a path to a dxil.dll

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
};
