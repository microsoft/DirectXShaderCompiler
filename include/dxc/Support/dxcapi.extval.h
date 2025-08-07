#include "dxc/Support/dxcapi.use.h"
#include <cassert>
#include <string>

namespace dxc {
class DxcDllExtValidationLoader : public DllLoader {
  // DxCompilerSupport manages the
  // lifetime of dxcompiler.dll, while DxilExtValSupport
  // manages the lifetime of dxil.dll
  dxc::SpecificDllLoader DxCompilerSupport;
  dxc::SpecificDllLoader DxilExtValSupport;

  std::string DxilDllPath;
  HRESULT InitializeInternal(LPCSTR fnName);

public:
  std::string GetDxilDllPath() { return DxilDllPath; }
  bool DxilDllFailedToLoad() {
    return !DxilDllPath.empty() && !DxilExtValSupport.IsEnabled();
  }

  HRESULT CreateInstanceImpl(REFCLSID clsid, REFIID riid,
                             IUnknown **pResult) override;
  HRESULT CreateInstance2Impl(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                              IUnknown **pResult) override;

  HRESULT Initialize() { return InitializeInternal("DxcCreateInstance"); }

  /* Note, OverrideDll takes this dll argument and ignores it
  to satisfy the IDllLoader interface. The parameter is ignored
  because the relevant dlls are specific and known: dxcompiler.dll
  and dxil.dll. This class is not designed to handle any other dlls */
  HRESULT OverrideDll(LPCSTR dll, LPCSTR entryPoint) override {
    return InitializeInternal(entryPoint);
  }

  bool HasCreateWithMalloc() const override {
    assert(DxCompilerSupport.HasCreateWithMalloc() &&
           DxilExtValSupport.HasCreateWithMalloc());
    return true;
  }

  bool IsEnabled() const override { return DxCompilerSupport.IsEnabled(); }

  void Cleanup() {
    DxilExtValSupport.Cleanup();
    DxCompilerSupport.Cleanup();
  }

  HMODULE Detach() override {
    // Can't Detach and return a handle for DxilSupport. Cleanup() instead.
    DxilExtValSupport.Cleanup();
    return DxCompilerSupport.Detach();
  }
};
} // namespace dxc