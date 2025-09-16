#include "dxc/Support/dxcapi.use.h"
#include "dxc/WinAdapter.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <string>

namespace dxc {

class ExternalValidationHelper3 : public IDxcCompiler3 {

public:
  CComPtr<IDxcCompiler3> m_pCompiler3;
  CComPtr<IDxcValidator> m_pValidator;

  ExternalValidationHelper3() {
    m_pCompiler3 = nullptr;
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
      _Out_ LPVOID *ppResult)
      override ///< IDxcResult: status, disassembly text, and errors.
  {
    return m_pCompiler3->Disassemble(pObject, riid, ppResult);
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    return m_pCompiler3->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return m_pCompiler3.p->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() override {
    return m_pCompiler3.p->Release();
  }

  HRESULT STDMETHODCALLTYPE Validate(
      IDxcBlob *pShader, // Shader to validate.
      UINT32 Flags,      // Validation flags.
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
  ) {
    return m_pValidator->Validate(pShader, Flags, ppResult);
  }

  ExternalValidationHelper3(CComPtr<IDxcCompiler3> pCompiler,
                            CComPtr<IDxcValidator> pValidator) {
    m_pCompiler3 = pCompiler;
    m_pValidator = pValidator;
  }
};

class ExternalValidationHelper : public IDxcCompiler {
public:
  CComPtr<IDxcCompiler> m_pCompiler;
  CComPtr<IDxcValidator> m_pValidator;

  ExternalValidationHelper() {
    m_pCompiler = nullptr;
    m_pValidator = nullptr;
  }
  HRESULT STDMETHODCALLTYPE Compile(
      _In_ IDxcBlob *pSource,         // Source text to compile.
      _In_opt_z_ LPCWSTR pSourceName, // Optional file name for pSource. Used in
                                      // errors and include handlers.
      _In_opt_z_ LPCWSTR pEntryPoint, // Entry point name.
      _In_z_ LPCWSTR pTargetProfile,  // Shader profile to compile.
      _In_opt_count_(argCount)
          LPCWSTR *pArguments, // Array of pointers to arguments.
      _In_ UINT32 argCount,    // Number of arguments.
      _In_count_(defineCount) const DxcDefine *pDefines, // Array of defines.
      _In_ UINT32 defineCount,                           // Number of defines.
      _In_opt_ IDxcIncludeHandler
          *pIncludeHandler, // User-provided interface to handle #include
                            // directives (optional).
      _COM_Outptr_ IDxcOperationResult *
          *ppResult // Compiler output status, buffer, and errors.
      ) override;

  /// \brief Preprocess source text.
  ///
  /// \deprecated Please use IDxcCompiler3::Compile() with the "-P" argument
  /// instead.
  HRESULT STDMETHODCALLTYPE Preprocess(
      _In_ IDxcBlob *pSource,         // Source text to preprocess.
      _In_opt_z_ LPCWSTR pSourceName, // Optional file name for pSource. Used in
                                      // errors and include handlers.
      _In_opt_count_(argCount)
          LPCWSTR *pArguments, // Array of pointers to arguments.
      _In_ UINT32 argCount,    // Number of arguments.
      _In_count_(defineCount) const DxcDefine *pDefines, // Array of defines.
      _In_ UINT32 defineCount,                           // Number of defines.
      _In_opt_ IDxcIncludeHandler
          *pIncludeHandler, // user-provided interface to handle #include
                            // directives (optional).
      _COM_Outptr_ IDxcOperationResult *
          *ppResult // Preprocessor output status, buffer, and errors.
      ) override {
    return m_pCompiler->Preprocess(pSource, pSourceName, pArguments, argCount,
                                   pDefines, defineCount, pIncludeHandler,
                                   ppResult);
  }

  /// \brief Disassemble a program.
  ///
  /// \deprecated Please use IDxcCompiler3::Disassemble() instead.
  HRESULT STDMETHODCALLTYPE Disassemble(
      _In_ IDxcBlob *pSource,                       // Program to disassemble.
      _COM_Outptr_ IDxcBlobEncoding **ppDisassembly // Disassembly text.
      ) override {
    return m_pCompiler->Disassemble(pSource, ppDisassembly);
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

  ExternalValidationHelper(CComPtr<IDxcCompiler> pCompiler,
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
