#include "dxc/Support/dxcapi.use.h"
#include "dxc/WinAdapter.h"
#include "llvm/Support/raw_ostream.h"

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
  int FailureReason = 0;

public:
  std::string getDxilDllPath() { return DxilDllPath; }
  int getFailureReason() { return FailureReason; }
  bool dxilDllFailedToLoad() {
    return !DxilDllPath.empty() && !DxilExtValSupport.IsEnabled();
  }

  HRESULT CreateInstanceImpl(REFCLSID clsid, REFIID riid,
                             IUnknown **pResult) override;
  HRESULT CreateInstance2Impl(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                              IUnknown **pResult) override;

  HRESULT initialize();

  bool IsEnabled() const override { return DxCompilerSupport.IsEnabled(); }
};
} // namespace dxc
