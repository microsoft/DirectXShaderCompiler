#include "dxc/Support/dxcapi.use.h"
#include <string>

namespace dxc {
class DxcDllExtValidationLoader : public DllLoader {
  // DxCompilerSupport manages the
  // lifetime of dxcompiler.dll, while DxilExtValSupport
  // manages the lifetime of dxil.dll
  dxc::SpecificDllLoader DxCompilerSupport;
  dxc::SpecificDllLoader DxilExtValSupport;

  DxcCreateInstanceProc m_createFn;
  DxcCreateInstance2Proc m_createFn2;

  std::string DxilDllPath;
  HRESULT InitializeInternal(LPCSTR fnName);

public:
  std::string GetDxilDllPath() { return DxilDllPath; }
  bool DxilDllFailedToLoad() {
    return !DxilDllPath.empty() && !DxilExtValSupport.IsEnabled();
  }

  HRESULT CreateInstance(REFCLSID clsid, REFIID riid, IUnknown **pResult);
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                          IUnknown **pResult);

  HRESULT Initialize() { return InitializeInternal("DxcCreateInstance"); }
  HRESULT InitializeForDll(LPCSTR dll, LPCSTR entryPoint) {
    return InitializeInternal(dll);
  }

  bool HasCreateWithMalloc() const { return m_createFn2 != nullptr; }

  bool IsEnabled() const { return DxCompilerSupport.IsEnabled(); }

  bool GetCreateInstanceProcs(DxcCreateInstanceProc *pCreateFn,
                              DxcCreateInstance2Proc *pCreateFn2) const;

  void Cleanup() {
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