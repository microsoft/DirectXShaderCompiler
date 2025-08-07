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

  HRESULT CreateInstanceImpl(REFCLSID clsid, REFIID riid, IUnknown **pResult);
  HRESULT CreateInstance2Impl(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                              IUnknown **pResult);

  HRESULT Initialize() { return InitializeInternal("DxcCreateInstance"); }

  /* Note, OverrideDll takes this dll argument and ignores it
  to satisfy the IDllLoader interface. The parameter is ignored
  because the relevant dlls are specific and known: dxcompiler.dll
  and dxil.dll. This class is not designed to handle any other dlls */
  HRESULT OverrideDll(LPCSTR dll, LPCSTR entryPoint) {
    return InitializeInternal(entryPoint);
  }

  bool HasCreateWithMalloc() const {
    assert(DxCompilerSupport.HasCreateWithMalloc() &&
           DxilExtValSupport.HasCreateWithMalloc());
    return true;
  }

  bool IsEnabled() const { return DxCompilerSupport.IsEnabled(); }

  void Cleanup() override {
    DxilExtValSupport.Cleanup();
    DxCompilerSupport.Cleanup();
  }

  HMODULE Detach() {
    // Can't Detach and return a handle for DxilSupport. Cleanup() instead.
    DxilExtValSupport.Cleanup();
    return DxCompilerSupport.Detach();
  }
};
} // namespace dxc