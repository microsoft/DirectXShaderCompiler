#ifndef _WIN32
#include "dxc/WinAdapter.h"
#endif

#include "dxc/Support/Global.h" // for hresult handling with DXC_FAILED
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/WinIncludes.h"
#include <filesystem> // C++17 and later
#include <sstream>
// WinIncludes must come before dxcapi.extval.h
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.extval.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.internal.h"
#include <iostream>

#include "llvm/ADT/SmallVector.h"

namespace {

// The ExtValidationArgHelper class helps manage arguments for external
// validation, as well as perform the validation step if needed.
class ExtValidationArgHelper {
public:
  std::vector<std::wstring> ArgStorage;
  llvm::SmallVector<LPCWSTR, 16> NewArgs;
  hlsl::options::DxcOpts Opts;
  CComPtr<IDxcValidator> Validator;
  UINT32 ValidatorVersionMajor = 0;
  UINT32 ValidatorVersionMinor = 0;
  bool NeedsValidation = false;
  bool RootSignatureOnly = false;

  ExtValidationArgHelper() = default;
  ExtValidationArgHelper(const ExtValidationArgHelper &) = delete;
  ExtValidationArgHelper(IDxcValidator *NewValidator)
      : ExtValidationArgHelper() {
    SetValidator(NewValidator);
  }
  ExtValidationArgHelper(IDxcValidator *NewValidator, LPCWSTR **Arguments,
                         UINT32 *ArgCount, LPCWSTR TargetProfile = nullptr)
      : ExtValidationArgHelper() {
    SetValidator(NewValidator);
    ProcessArgs(Arguments, ArgCount, TargetProfile);
  }

  /// Add argument to ArgStorage to be referenced by NewArgs.
  void AddArgument(const std::wstring &Arg) { ArgStorage.push_back(Arg); }

  /// Retrieve validator version.
  void SetValidator(IDxcValidator *NewValidator) {
    Validator = NewValidator;
    IFTARG(Validator);

    // 1.0 had no version interface.
    ValidatorVersionMajor = 1;
    ValidatorVersionMinor = 0;
    CComPtr<IDxcVersionInfo> pValidatorVersionInfo;
    if (SUCCEEDED(
            Validator->QueryInterface(IID_PPV_ARGS(&pValidatorVersionInfo)))) {
      IFT(pValidatorVersionInfo->GetVersion(&ValidatorVersionMajor,
                                            &ValidatorVersionMinor));
    }
  }

  /// Process arguments, adding validator version and disable validation if
  /// needed. If TargetProfile provided, use this instead of -T argument.
  void ProcessArgs(LPCWSTR **Arguments, UINT32 *ArgCount,
                   LPCWSTR TargetProfile = nullptr) {
    IFTARG((Arguments && ArgCount) && (*Arguments || *ArgCount == 0) &&
           *ArgCount < INT_MAX - 3);
    NeedsValidation = false;

    // Must not call twice.
    IFTBOOL(NewArgs.empty(), E_FAIL);

    // Parse compiler arguments to Opts.
    std::string Errors;
    llvm::raw_string_ostream OS(Errors);
    hlsl::options::MainArgs MainArgs(static_cast<int>(*ArgCount), *Arguments,
                                     0);
    if (hlsl::options::ReadDxcOpts(hlsl::options::getHlslOptTable(),
                                   hlsl::options::CompilerFlags, MainArgs, Opts,
                                   OS))
      return; // error - allow compiler to emit error.

    // Determine important conditions from arguments.
    bool ValidatorVersionNeeded = Opts.ValVerMajor == UINT_MAX;
    bool ProduceDxModule = !Opts.AstDump && !Opts.OptDump &&
#ifdef ENABLE_SPIRV_CODEGEN
                           !Opts.GenSPIRV &&
#endif
                           !Opts.DumpDependencies && !Opts.VerifyDiagnostics &&
                           Opts.Preprocess.empty();
    bool ProduceFullContainer = ProduceDxModule && !Opts.CodeGenHighLevel;
    NeedsValidation = ProduceFullContainer && !Opts.DisableValidation;

    // Check target profile.
    if ((TargetProfile &&
         std::wstring(TargetProfile).compare(0, 8, L"rootsig_") == 0) ||
        Opts.TargetProfile.startswith("rootsig_")) {
      RootSignatureOnly = true;
      if (ValidatorVersionMajor == 1 && ValidatorVersionMinor < 5)
        NeedsValidation = false; // No rootsig validation before 1.5
    }

    // Add extra arguments as needed
    if (ProduceDxModule) {
      if (ValidatorVersionNeeded) {
        // If not supplied, add validator version arguments.
        AddArgument(L"-validator-version");
        std::wstring verStr = std::to_wstring(ValidatorVersionMajor) + L"." +
                              std::to_wstring(ValidatorVersionMinor);
        AddArgument(verStr);
      }

      // Add argument to disable validation, so we can call it ourselves
      // later.
      if (NeedsValidation)
        AddArgument(L"-Vd");
    }

    if (!ArgStorage.size())
      return;

    // Reference added arguments from storage.
    NewArgs.reserve(*ArgCount + ArgStorage.size());

    // Copied arguments refer to original strings.
    for (UINT32 i = 0; i < *ArgCount; ++i)
      NewArgs.push_back((*Arguments)[i]);

    for (const auto &Arg : ArgStorage)
      NewArgs.push_back(Arg.c_str());

    *Arguments = NewArgs.data();
    *ArgCount = (UINT32)NewArgs.size();

    return;
  }

  HRESULT DoValidation(IDxcOperationResult *CompileResult, REFIID riid,
                       void **ppResult) {
    // this lambda takes an arbitrary object that implements the
    // IDxcOperationResult interface, asks whether that object
    // also implements the interface specified by riid, and if
    // so, sets ppResult to point to that object
    auto UseResult = [&](IDxcOperationResult *Result) -> HRESULT {
      if (Result)
        return Result->QueryInterface(riid, ppResult);
      else
        return E_FAIL;
    };

    // No validation needed; just set the result and return.
    if (!NeedsValidation)
      return UseResult(CompileResult);

    // Get the compiled shader.
    CComPtr<IDxcBlob> pCompiledBlob;
    IFR(CompileResult->GetResult(&pCompiledBlob));

    // If no compiled blob; just return the compile result.
    if (!pCompiledBlob)
      return UseResult(CompileResult);

    // Validate the compiled shader.
    CComPtr<IDxcOperationResult> ValidationResult;
    UINT32 DxcValidatorFlags =
        DxcValidatorFlags_InPlaceEdit |
        (RootSignatureOnly ? DxcValidatorFlags_RootSignatureOnly : 0);
    IFR(Validator->Validate(pCompiledBlob, DxcValidatorFlags,
                            &ValidationResult));

    // Return the validation result if it failed.
    HRESULT HR;
    IFR(ValidationResult->GetStatus(&HR));
    if (FAILED(HR))
      return UseResult(ValidationResult);

    // Validation succeeded. Return the original compile result.
    return UseResult(CompileResult);
  }
};

// ExternalValidationCompiler provides a DxCompiler that performs validation
// using an external validator instead of an internal one.
// It uses a wrapping approach, where it wraps the provided compiler object,
// adds '-Vd' and '-validator-version' when appropriate to skip internal
// validation and set the default validator version, then uses the provided
// IDxcValidator interface to validate a successful compilation result, when
// validation would normally be performed.
class ExternalValidationCompiler : public IDxcCompiler2,
                                   public IDxcCompiler3,
                                   public IDxcLangExtensions3,
                                   public IDxcContainerEvent,
                                   public IDxcVersionInfo3,
                                   public IDxcVersionInfo2 {
private:
  CComPtr<IDxcValidator> m_pValidator;

  // This wrapper wraps one particular compiler interface.
  // When QueryInterface is called, we create a new wrapper
  // for the requested interface, which wraps the result of QueryInterface
  // on the compiler object. Compiler pointer is held as IUnknown and must
  // be upcast to the appropriate interface on use.
  IID m_CompilerIID;
  CComPtr<IUnknown> m_pCompiler;

public:
  ExternalValidationCompiler(IMalloc *pMalloc, IDxcValidator *pValidator,
                             REFIID CompilerIID, IUnknown *pCompiler)
      : m_pValidator(pValidator), m_CompilerIID(CompilerIID),
        m_pCompiler(pCompiler), m_pMalloc(pMalloc) {}

  // IUnknown implementation
  DXC_MICROCOM_TM_REF_FIELDS()
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(ExternalValidationCompiler)

  // QueryInterface creates a new wrapper for the correct compiler interface
  // retrieved by calling QueryInterface on the compiler. The new wrapper
  // keeps the IID and returned compiler interface pointer for use in
  // associated interface methods. Those methods can then use
  // castCompilerUnsafe to upcast the pointer to the appropriate interface
  // for the method.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    if (!ppvObject)
      return E_POINTER;

    *ppvObject = nullptr;

    // Get pointer for an interface the compiler object supports.
    CComPtr<IUnknown> Compiler;
    IFR(m_pCompiler->QueryInterface(iid, (void **)&Compiler));

    try {
      // This can throw; do not leak C++ exceptions through COM method
      // calls.
      CComPtr<ExternalValidationCompiler> NewWrapper(
          Alloc(m_pMalloc, m_pValidator, iid, Compiler));
      return DoBasicQueryInterface<
          IDxcCompiler, IDxcCompiler2, IDxcCompiler3, IDxcLangExtensions,
          IDxcLangExtensions2, IDxcLangExtensions3, IDxcContainerEvent,
          IDxcVersionInfo, IDxcVersionInfo2, IDxcVersionInfo3>(NewWrapper.p,
                                                               iid, ppvObject);
    } catch (...) {
      return E_FAIL;
    }
  }

  // Cast current compiler interface pointer. Used from methods of the
  // associated interface, assuming that the current compiler interface is
  // correct for the method call.
  // This will either be casting to the original interface retrieved by
  // QueryInterface, or to one from which that interface derives.
  template <typename T> T *castCompilerUnsafe() {
    return static_cast<T *>(m_pCompiler.p);
  }

  template <typename T> T *castCompilerSafe() const {
    // Compare stored IID with the IID of T
    if (m_CompilerIID == __uuidof(T)) {
      // Safe to cast because the underlying compiler object in
      // m_pCompiler originally implemented the interface T
      return static_cast<T *>(m_pCompiler.p);
    }

    return nullptr;
  }

  // IDxcCompiler implementation
  HRESULT STDMETHODCALLTYPE Compile(IDxcBlob *pSource, LPCWSTR pSourceName,
                                    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
                                    LPCWSTR *pArguments, UINT32 argCount,
                                    const DxcDefine *pDefines,
                                    UINT32 defineCount,
                                    IDxcIncludeHandler *pIncludeHandler,
                                    IDxcOperationResult **ppResult) override {
    if (ppResult == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      // ProcessArgs will update pArguments and argCount if needed.
      ExtValidationArgHelper Helper(m_pValidator, &pArguments, &argCount,
                                    pTargetProfile);

      CComPtr<IDxcOperationResult> CompileResult;
      IFR(castCompilerUnsafe<IDxcCompiler>()->Compile(
          pSource, pSourceName, pEntryPoint, pTargetProfile, pArguments,
          argCount, pDefines, defineCount, pIncludeHandler, &CompileResult));

      return Helper.DoValidation(CompileResult, IID_PPV_ARGS(ppResult));

    } catch (std::bad_alloc &) {
      return E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      assert(DXC_FAILED(e.hr));
      return DxcResult::Create(
          e.hr, DXC_OUT_NONE,
          {DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())},
          ppResult);
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  Preprocess(IDxcBlob *pSource, LPCWSTR pSourceName, LPCWSTR *pArguments,
             UINT32 argCount, const DxcDefine *pDefines, UINT32 defineCount,
             IDxcIncludeHandler *pIncludeHandler,
             IDxcOperationResult **ppResult) override {
    return castCompilerUnsafe<IDxcCompiler>()->Preprocess(
        pSource, pSourceName, pArguments, argCount, pDefines, defineCount,
        pIncludeHandler, ppResult);
  }

  HRESULT STDMETHODCALLTYPE
  Disassemble(IDxcBlob *pSource, IDxcBlobEncoding **ppDisassembly) override {
    return castCompilerUnsafe<IDxcCompiler>()->Disassemble(pSource,
                                                           ppDisassembly);
  }

  // IDxcCompiler2 implementation
  HRESULT STDMETHODCALLTYPE CompileWithDebug(
      IDxcBlob *pSource, LPCWSTR pSourceName, LPCWSTR pEntryPoint,
      LPCWSTR pTargetProfile, LPCWSTR *pArguments, UINT32 argCount,
      const DxcDefine *pDefines, UINT32 defineCount,
      IDxcIncludeHandler *pIncludeHandler, IDxcOperationResult **ppResult,
      LPWSTR *ppDebugBlobName, IDxcBlob **ppDebugBlob) override {
    if (ppResult == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      // ProcessArgs will update pArguments and argCount if needed.
      ExtValidationArgHelper Helper(m_pValidator, &pArguments, &argCount,
                                    pTargetProfile);

      CComPtr<IDxcOperationResult> CompileResult;
      IFR(castCompilerUnsafe<IDxcCompiler2>()->CompileWithDebug(
          pSource, pSourceName, pEntryPoint, pTargetProfile, pArguments,
          argCount, pDefines, defineCount, pIncludeHandler, &CompileResult,
          ppDebugBlobName, ppDebugBlob));

      return Helper.DoValidation(CompileResult, IID_PPV_ARGS(ppResult));

    } catch (std::bad_alloc &) {
      return E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      assert(DXC_FAILED(e.hr));
      return DxcResult::Create(
          e.hr, DXC_OUT_NONE,
          {DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())},
          ppResult);
    } catch (...) {
      return E_FAIL;
    }
  }

  // IDxcCompiler3 implementation
  HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer *pSource,
                                    LPCWSTR *pArguments, UINT32 argCount,
                                    IDxcIncludeHandler *pIncludeHandler,
                                    REFIID riid, LPVOID *ppResult) override {
    if (ppResult == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      // ProcessArgs will update pArguments and argCount if needed.
      ExtValidationArgHelper Helper(m_pValidator, &pArguments, &argCount);

      CComPtr<IDxcResult> CompileResult;
      IFR(castCompilerUnsafe<IDxcCompiler3>()->Compile(
          pSource, pArguments, argCount, pIncludeHandler,
          IID_PPV_ARGS(&CompileResult)));

      return Helper.DoValidation(CompileResult, riid, ppResult);

    } catch (std::bad_alloc &) {
      return E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      assert(DXC_FAILED(e.hr));
      CComPtr<IDxcResult> errorResult;
      return DxcResult::Create(
          e.hr, DXC_OUT_NONE,
          {DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())},
          &errorResult);
      return errorResult->QueryInterface(riid, ppResult);
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE Disassemble(const DxcBuffer *pObject, REFIID riid,
                                        LPVOID *ppResult) override {
    return castCompilerUnsafe<IDxcCompiler3>()->Disassemble(pObject, riid,
                                                            ppResult);
  }

  // IDxcLangExtensions pass-through implementations
  HRESULT STDMETHODCALLTYPE RegisterSemanticDefine(LPCWSTR name) override {
    return castCompilerUnsafe<IDxcLangExtensions>()->RegisterSemanticDefine(
        name);
  }
  HRESULT STDMETHODCALLTYPE
  RegisterSemanticDefineExclusion(LPCWSTR name) override {
    return castCompilerUnsafe<IDxcLangExtensions>()
        ->RegisterSemanticDefineExclusion(name);
  }
  HRESULT STDMETHODCALLTYPE RegisterDefine(LPCWSTR name) override {
    return castCompilerUnsafe<IDxcLangExtensions>()->RegisterDefine(name);
  }
  HRESULT STDMETHODCALLTYPE
  RegisterIntrinsicTable(IDxcIntrinsicTable *pTable) override {
    return castCompilerUnsafe<IDxcLangExtensions>()->RegisterIntrinsicTable(
        pTable);
  }
  HRESULT STDMETHODCALLTYPE
  SetSemanticDefineValidator(IDxcSemanticDefineValidator *pValidator) override {
    return castCompilerUnsafe<IDxcLangExtensions>()->SetSemanticDefineValidator(
        pValidator);
  }
  HRESULT STDMETHODCALLTYPE
  SetSemanticDefineMetaDataName(LPCSTR name) override {
    return castCompilerUnsafe<IDxcLangExtensions>()
        ->SetSemanticDefineMetaDataName(name);
  }
  // IDxcLangExtensions2 pass-through implementations
  HRESULT STDMETHODCALLTYPE SetTargetTriple(LPCSTR name) override {
    return castCompilerUnsafe<IDxcLangExtensions2>()->SetTargetTriple(name);
  }
  // IDxcLangExtensions3 pass-through implementations
  HRESULT STDMETHODCALLTYPE
  RegisterNonOptSemanticDefine(LPCWSTR name) override {
    return castCompilerUnsafe<IDxcLangExtensions3>()
        ->RegisterNonOptSemanticDefine(name);
  }

  // IDxcContainerEvent pass-through implementations
  HRESULT STDMETHODCALLTYPE RegisterDxilContainerEventHandler(
      IDxcContainerEventsHandler *pHandler, UINT64 *pCookie) override {
    return castCompilerUnsafe<IDxcContainerEvent>()
        ->RegisterDxilContainerEventHandler(pHandler, pCookie);
  }
  HRESULT STDMETHODCALLTYPE
  UnRegisterDxilContainerEventHandler(UINT64 cookie) override {
    return castCompilerUnsafe<IDxcContainerEvent>()
        ->UnRegisterDxilContainerEventHandler(cookie);
  }

  // IDxcVersionInfo3 pass-through implementations
  HRESULT STDMETHODCALLTYPE
  GetCustomVersionString(char **pVersionString) override {
    return castCompilerUnsafe<IDxcVersionInfo3>()->GetCustomVersionString(
        pVersionString);
  }

  // IDxcVersionInfo2 pass-through implementations
  HRESULT STDMETHODCALLTYPE GetCommitInfo(UINT32 *pCommitCount,
                                          char **pCommitHash) override {
    return castCompilerUnsafe<IDxcVersionInfo2>()->GetCommitInfo(pCommitCount,
                                                                 pCommitHash);
  }

  // IDxcVersionInfo pass-through implementations
  HRESULT STDMETHODCALLTYPE GetVersion(UINT32 *pMajor,
                                       UINT32 *pMinor) override {
    return castCompilerUnsafe<IDxcVersionInfo>()->GetVersion(pMajor, pMinor);
  }
  HRESULT STDMETHODCALLTYPE GetFlags(UINT32 *pFlags) override {
    return castCompilerUnsafe<IDxcVersionInfo>()->GetFlags(pFlags);
  }
};
} // namespace

namespace dxc {

static HRESULT CreateCompilerWrapper(IMalloc *pMalloc,
                                     SpecificDllLoader &DxCompilerSupport,
                                     SpecificDllLoader &DxilExtValSupport,
                                     REFIID riid, IUnknown **ppResult) {
  IFTARG(ppResult);
  *ppResult = nullptr;

  CComPtr<IUnknown> Compiler;
  CComPtr<IDxcValidator> Validator;

  // Create compiler and validator
  if (pMalloc) {
    IFR(DxCompilerSupport.CreateInstance2(pMalloc, CLSID_DxcCompiler, riid,
                                          &Compiler));
    IFR(DxilExtValSupport.CreateInstance2<IDxcValidator>(
        pMalloc, CLSID_DxcValidator, &Validator));
  } else {
    IFR(DxCompilerSupport.CreateInstance(CLSID_DxcCompiler, riid, &Compiler));
    IFR(DxilExtValSupport.CreateInstance<IDxcValidator>(CLSID_DxcValidator,
                                                        &Validator));
  }

  // Wrap compiler
  CComPtr<ExternalValidationCompiler> CompilerWrapper =
      ExternalValidationCompiler::Alloc(pMalloc ? pMalloc
                                                : DxcGetThreadMallocNoRef(),
                                        Validator, riid, Compiler);
  return CompilerWrapper->QueryInterface(riid, (void **)ppResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstanceImpl(REFCLSID clsid,
                                                      REFIID riid,
                                                      IUnknown **pResult) {
  if (!pResult)
    return E_POINTER;

  *pResult = nullptr;

  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance(clsid, riid, pResult);
    }
    if (clsid == CLSID_DxcCompiler) {
      return CreateCompilerWrapper(nullptr, DxCompilerSupport,
                                   DxilExtValSupport, riid, pResult);
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance(clsid, riid, pResult);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2Impl(IMalloc *pMalloc,
                                                       REFCLSID clsid,
                                                       REFIID riid,
                                                       IUnknown **pResult) {
  if (!pResult)
    return E_POINTER;

  *pResult = nullptr;
  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance2(pMalloc, clsid, riid, pResult);
    }
    if (clsid == CLSID_DxcCompiler) {
      return CreateCompilerWrapper(pMalloc, DxCompilerSupport,
                                   DxilExtValSupport, riid, pResult);
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance2(pMalloc, clsid, riid, pResult);
}

HRESULT
DxcDllExtValidationLoader::Initialize(llvm::raw_string_ostream &log) {
  // Load dxcompiler.dll
  HRESULT Result =
      DxCompilerSupport.InitializeForDll(kDxCompilerLib, "DxcCreateInstance");
  // if dxcompiler.dll fails to load, return the failed HRESULT
  if (DXC_FAILED(Result)) {
    log << "dxcompiler.dll failed to load";
    return Result;
  }

  // now handle external dxil.dll
  const char *EnvVarVal = std::getenv("DXC_DXIL_DLL_PATH");
  if (!EnvVarVal || std::string(EnvVarVal).empty()) {
    // no need to emit anything if external validation isn't wanted
    return S_OK;
  }

  DxilDllPath = std::string(EnvVarVal);
  std::filesystem::path DllPath(DxilDllPath);

  // Check if path is absolute and exists
  if (!DllPath.is_absolute() || !std::filesystem::exists(DllPath)) {
    log << "dxil.dll path " << DxilDllPath << " could not be found";
    return E_INVALIDARG;
  }

  log << "Loading external dxil.dll from " << DxilDllPath;
  Result = DxilExtValSupport.InitializeForDll(DxilDllPath.c_str(),
                                              "DxcCreateInstance");
  if (DXC_FAILED(Result)) {
    log << "dxil.dll failed to load";
    return Result;
  }

  return Result;
}

} // namespace dxc
