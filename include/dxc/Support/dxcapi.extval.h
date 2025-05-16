#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.h"

using dxc::DxcDllSupport;

class DxcDllExtValidationSupport : public DxcDllSupport {
  // DxcDllExtValidationSupport manages the
  // lifetime of dxcompiler.dll, while the member, m_DxilSupport,
  // manages the lifetime of dxil.dll
  DxcDllSupport m_DxilSupport;

  std::string DxilDllPath;
  // override DxcDllSupport's implementation of InitializeInternal,
  // adding the environment variable value check for a path to a dxil.dll
  HRESULT InitializeInternal(LPCSTR dllName, LPCSTR fnName);

  std::string GetDxilDLLPathExt() { return DxilDllPath; }
  bool DxilDllSuccessfullyLoaded() {
    return !DxilDllPath.empty() && !m_DxilSupport.IsEnabled();
  }

  HRESULT InitializeInternal(LPCSTR dllName, LPCSTR fnName) {
    // Load dxcompiler.dll
    HRESULT result = InitializeForDll(dllName, fnName);
    // if dxcompiler.dll fails to load, then we won't try loading dxil.dll,
    // so set this boolean to false.

    // now handle external dxil.dll
    const char *envVal = std::getenv("DXC_DXIL_DLL_PATH");
    bool ValidateInternally = false;
    if (!envVal || std::string(envVal).empty()) {
      return S_OK;
    }

    std::string DllPathStr(envVal);
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
    m_DxilSupport.InitializeForDll(DllPathStr.data(), fnName);

    return S_OK;
  }
};