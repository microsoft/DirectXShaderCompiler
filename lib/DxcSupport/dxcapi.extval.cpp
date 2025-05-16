#include "dxc/Support/WinIncludes.h"

#include "dxc/Support/dxcapi.use.h"

#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/SharedLibAffix.h" // header generated during DXC build
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinFunctions.h"

#include "dxc/Support/dxcapi.extval.h"

HRESULT DxcDllExtValidationSupport::InitializeInternal(LPCSTR dllName,
                                                       LPCSTR fnName) {
  // Load dxcompiler.dll
  HRESULT result = InitializeForDll(dllName, fnName);
  // if dxcompiler.dll fails to load, return the failed HRESULT
  if (DXC_FAILED(result)) {
    return result;
  }

  // now handle external dxil.dll
  const char *envVal = std::getenv("DXC_DXIL_DLL_PATH");
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