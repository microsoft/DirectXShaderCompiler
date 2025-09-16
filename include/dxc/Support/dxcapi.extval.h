#include "dxc/Support/dxcapi.use.h"
#include "dxc/WinAdapter.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <string>

namespace dxc {

class ExternalValidationHelper : public IDxcCompiler3 {

public:
  CComPtr<IDxcCompiler3> m_pCompiler;
  CComPtr<IDxcValidator> m_pValidator;

  ExternalValidationHelper() {
    m_pCompiler = nullptr;
    m_pValidator = nullptr;
  }

  HRESULT STDMETHODCALLTYPE Compile(
      _In_ const DxcBuffer *pSource, ///< Source text to compile.
      _In_opt_count_(argCount)
          LPCWSTR *pArguments, ///< Array of pointers to arguments.
      _In_ UINT32 argCount,    ///< Number of arguments.
      _In_opt_ IDxcIncludeHandler
          *pIncludeHandler,  ///< user-provided interface to handle include
                             ///< directives (optional).
      _In_ REFIID riid,      ///< Interface ID for the result.
      _Out_ LPVOID *ppResult ///< IDxcResult: status, buffer, and errors.
      ) override;

  HRESULT STDMETHODCALLTYPE Disassemble(
      _In_ const DxcBuffer
          *pObject,     ///< Program to disassemble: dxil container or bitcode.
      _In_ REFIID riid, ///< Interface ID for the result.
      _Out_ LPVOID
          *ppResult) ///< IDxcResult: status, disassembly text, and errors.
  {
    return m_pCompiler->Disassemble(pObject, riid, ppResult);
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    return m_pCompiler->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return m_pCompiler.p->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() override {
    return m_pCompiler.p->Release();
  }
  HRESULT STDMETHODCALLTYPE Validate(
      IDxcBlob *pShader, // Shader to validate.
      UINT32 Flags,      // Validation flags.
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
  ) {
    return m_pValidator->Validate(pShader, Flags, ppResult);
  }

  ExternalValidationHelper(CComPtr<IDxcCompiler3> pCompiler,
                           CComPtr<IDxcValidator> pValidator) {
    m_pCompiler = pCompiler;
    m_pValidator = pValidator;
  }
};

class DxcDllExtValidationLoader : public DllLoader {
  // DxCompilerSupport manages the
  // lifetime of dxcompiler.dll, while DxilExtValSupport
  // manages the lifetime of dxil.dll
  dxc::SpecificDllLoader DxCompilerSupport;
  dxc::SpecificDllLoader DxilExtValSupport;
  std::string DxilDllPath;

public:
  std::string GetDxilDllPath() { return DxilDllPath; }
  bool DxilDllFailedToLoad() {
    return !DxilDllPath.empty() && !DxilExtValSupport.IsEnabled();
  }

  HRESULT CreateInstanceImpl(REFCLSID clsid, REFIID riid,
                             IUnknown **pResult) override;
  HRESULT CreateInstance2Impl(IMalloc *pMalloc, REFCLSID clsid, REFIID riid,
                              IUnknown **pResult) override;

  HRESULT Initialize(llvm::raw_string_ostream &log);

  bool IsEnabled() const override { return DxCompilerSupport.IsEnabled(); }
};
} // namespace dxc
