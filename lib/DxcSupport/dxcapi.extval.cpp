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
    CComPtr<IDxcVersionInfo> ValidatorVersionInfo;
    if (SUCCEEDED(
            Validator->QueryInterface(IID_PPV_ARGS(&ValidatorVersionInfo)))) {
      IFT(ValidatorVersionInfo->GetVersion(&ValidatorVersionMajor,
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
                       void **ValResult) {
    // this lambda takes an arbitrary object that implements the
    // IDxcOperationResult interface, asks whether that object
    // also implements the interface specified by riid, and if
    // so, sets ValResult to point to that object
    auto UseResult = [&](IDxcOperationResult *Result) -> HRESULT {
      if (Result)
        return Result->QueryInterface(riid, ValResult);
      else
        return E_FAIL;
    };

    // No validation needed; just set the result and return.
    if (!NeedsValidation)
      return UseResult(CompileResult);

    // Get the compiled shader.
    CComPtr<IDxcBlob> CompiledBlob;
    IFR(CompileResult->GetResult(&CompiledBlob));

    // If no compiled blob; just return the compile result.
    if (!CompiledBlob)
      return UseResult(CompileResult);

    // Validate the compiled shader.
    CComPtr<IDxcOperationResult> TempValidationResult;
    UINT32 DxcValidatorFlags =
        DxcValidatorFlags_InPlaceEdit |
        (RootSignatureOnly ? DxcValidatorFlags_RootSignatureOnly : 0);
    IFR(Validator->Validate(CompiledBlob, DxcValidatorFlags,
                            &TempValidationResult));

    // Return the validation result if it failed.
    HRESULT HR;
    IFR(TempValidationResult->GetStatus(&HR));
    if (FAILED(HR))
      return UseResult(TempValidationResult);

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
  CComPtr<IDxcValidator> Validator;

  // This wrapper wraps one particular compiler interface.
  // When QueryInterface is called, we create a new wrapper
  // for the requested interface, which wraps the result of QueryInterface
  // on the compiler object. Compiler pointer is held as IUnknown and must
  // be upcast to the appropriate interface on use.
  IID CompilerIID;
  CComPtr<IUnknown> Compiler;

public:
  ExternalValidationCompiler(IMalloc *Malloc, IDxcValidator *OtherValidator,
                             REFIID OtherCompilerIID, IUnknown *OtherCompiler)
      : Validator(OtherValidator), CompilerIID(OtherCompilerIID),
        Compiler(OtherCompiler), m_pMalloc(Malloc) {}

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
                                           void **ResultObject) override {
    if (!ResultObject)
      return E_POINTER;

    *ResultObject = nullptr;

    // Get pointer for an interface the compiler object supports.
    CComPtr<IUnknown> TempCompiler;
    IFR(Compiler->QueryInterface(iid, (void **)&TempCompiler));

    try {
      // This can throw; do not leak C++ exceptions through COM method
      // calls.
      CComPtr<ExternalValidationCompiler> NewWrapper(
          Alloc(m_pMalloc, Validator, iid, TempCompiler));
      return DoBasicQueryInterface<
          IDxcCompiler, IDxcCompiler2, IDxcCompiler3, IDxcLangExtensions,
          IDxcLangExtensions2, IDxcLangExtensions3, IDxcContainerEvent,
          IDxcVersionInfo, IDxcVersionInfo2, IDxcVersionInfo3>(
          NewWrapper.p, iid, ResultObject);
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
    return static_cast<T *>(Compiler.p);
  }

  template <typename T> T *castCompilerSafe() const {
    // Compare stored IID with the IID of T
    if (CompilerIID == __uuidof(T)) {
      // Safe to cast because the underlying compiler object in
      // Compiler originally implemented the interface T
      return static_cast<T *>(Compiler.p);
    }

    return nullptr;
  }

  // IDxcCompiler implementation
  HRESULT STDMETHODCALLTYPE
  Compile(IDxcBlob *Source, LPCWSTR SourceName, LPCWSTR EntryPoint,
          LPCWSTR TargetProfile, LPCWSTR *Arguments, UINT32 argCount,
          const DxcDefine *Defines, UINT32 defineCount,
          IDxcIncludeHandler *IncludeHandler,
          IDxcOperationResult **ResultObject) override {
    if (ResultObject == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      // ProcessArgs will update pArguments and argCount if needed.
      ExtValidationArgHelper Helper(Validator, &Arguments, &argCount,
                                    TargetProfile);

      CComPtr<IDxcOperationResult> CompileResult;
      IFR(castCompilerUnsafe<IDxcCompiler>()->Compile(
          Source, SourceName, EntryPoint, TargetProfile, Arguments, argCount,
          Defines, defineCount, IncludeHandler, &CompileResult));

      return Helper.DoValidation(CompileResult, IID_PPV_ARGS(ResultObject));

    } catch (std::bad_alloc &) {
      return E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      assert(DXC_FAILED(e.hr));
      return DxcResult::Create(
          e.hr, DXC_OUT_NONE,
          {DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())},
          ResultObject);
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  Preprocess(IDxcBlob *Source, LPCWSTR SourceName, LPCWSTR *Arguments,
             UINT32 argCount, const DxcDefine *Defines, UINT32 defineCount,
             IDxcIncludeHandler *IncludeHandler,
             IDxcOperationResult **ResultObject) override {
    return castCompilerUnsafe<IDxcCompiler>()->Preprocess(
        Source, SourceName, Arguments, argCount, Defines, defineCount,
        IncludeHandler, ResultObject);
  }

  HRESULT STDMETHODCALLTYPE
  Disassemble(IDxcBlob *Source, IDxcBlobEncoding **Disassembly) override {
    return castCompilerUnsafe<IDxcCompiler>()->Disassemble(Source, Disassembly);
  }

  // IDxcCompiler2 implementation
  HRESULT STDMETHODCALLTYPE CompileWithDebug(
      IDxcBlob *Source, LPCWSTR SourceName, LPCWSTR EntryPoint,
      LPCWSTR TargetProfile, LPCWSTR *Arguments, UINT32 argCount,
      const DxcDefine *pDefines, UINT32 defineCount,
      IDxcIncludeHandler *IncludeHandler, IDxcOperationResult **ResultObject,
      LPWSTR *DebugBlobName, IDxcBlob **DebugBlob) override {
    if (ResultObject == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      // ProcessArgs will update Arguments and argCount if needed.
      ExtValidationArgHelper Helper(Validator, &Arguments, &argCount,
                                    TargetProfile);

      CComPtr<IDxcOperationResult> CompileResult;
      IFR(castCompilerUnsafe<IDxcCompiler2>()->CompileWithDebug(
          Source, SourceName, EntryPoint, TargetProfile, Arguments, argCount,
          pDefines, defineCount, IncludeHandler, &CompileResult, DebugBlobName,
          DebugBlob));

      return Helper.DoValidation(CompileResult, IID_PPV_ARGS(ResultObject));

    } catch (std::bad_alloc &) {
      return E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      assert(DXC_FAILED(e.hr));
      return DxcResult::Create(
          e.hr, DXC_OUT_NONE,
          {DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())},
          ResultObject);
    } catch (...) {
      return E_FAIL;
    }
  }

  // IDxcCompiler3 implementation
  HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer *Source, LPCWSTR *Arguments,
                                    UINT32 argCount,
                                    IDxcIncludeHandler *IncludeHandler,
                                    REFIID riid,
                                    LPVOID *ResultObject) override {
    if (ResultObject == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      // ProcessArgs will update Arguments and argCount if needed.
      ExtValidationArgHelper Helper(Validator, &Arguments, &argCount);

      CComPtr<IDxcResult> CompileResult;
      IFR(castCompilerUnsafe<IDxcCompiler3>()->Compile(
          Source, Arguments, argCount, IncludeHandler,
          IID_PPV_ARGS(&CompileResult)));

      return Helper.DoValidation(CompileResult, riid, ResultObject);

    } catch (std::bad_alloc &) {
      return E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      assert(DXC_FAILED(e.hr));
      CComPtr<IDxcResult> errorResult;
      return DxcResult::Create(
          e.hr, DXC_OUT_NONE,
          {DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())},
          &errorResult);
      return errorResult->QueryInterface(riid, ResultObject);
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE Disassemble(const DxcBuffer *Object, REFIID riid,
                                        LPVOID *ResultObject) override {
    return castCompilerUnsafe<IDxcCompiler3>()->Disassemble(Object, riid,
                                                            ResultObject);
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
  SetSemanticDefineValidator(IDxcSemanticDefineValidator *Validator) override {
    return castCompilerUnsafe<IDxcLangExtensions>()->SetSemanticDefineValidator(
        Validator);
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
      IDxcContainerEventsHandler *Handler, UINT64 *Cookie) override {
    return castCompilerUnsafe<IDxcContainerEvent>()
        ->RegisterDxilContainerEventHandler(Handler, Cookie);
  }
  HRESULT STDMETHODCALLTYPE
  UnRegisterDxilContainerEventHandler(UINT64 cookie) override {
    return castCompilerUnsafe<IDxcContainerEvent>()
        ->UnRegisterDxilContainerEventHandler(cookie);
  }

  // IDxcVersionInfo3 pass-through implementations
  HRESULT STDMETHODCALLTYPE
  GetCustomVersionString(char **VersionString) override {
    return castCompilerUnsafe<IDxcVersionInfo3>()->GetCustomVersionString(
        VersionString);
  }

  // IDxcVersionInfo2 pass-through implementations
  HRESULT STDMETHODCALLTYPE GetCommitInfo(UINT32 *CommitCount,
                                          char **CommitHash) override {
    return castCompilerUnsafe<IDxcVersionInfo2>()->GetCommitInfo(CommitCount,
                                                                 CommitHash);
  }

  // IDxcVersionInfo pass-through implementations
  HRESULT STDMETHODCALLTYPE GetVersion(UINT32 *Major, UINT32 *Minor) override {
    return castCompilerUnsafe<IDxcVersionInfo>()->GetVersion(Major, Minor);
  }
  HRESULT STDMETHODCALLTYPE GetFlags(UINT32 *Flags) override {
    return castCompilerUnsafe<IDxcVersionInfo>()->GetFlags(Flags);
  }
};
} // namespace

namespace dxc {

static HRESULT CreateCompilerWrapper(IMalloc *Malloc,
                                     SpecificDllLoader &DxCompilerSupport,
                                     SpecificDllLoader &DxilExtValSupport,
                                     REFIID riid, IUnknown **ResultObject) {
  IFTARG(ResultObject);
  *ResultObject = nullptr;

  CComPtr<IUnknown> Compiler;
  CComPtr<IDxcValidator> Validator;

  // Create compiler and validator
  if (Malloc) {
    IFR(DxCompilerSupport.CreateInstance2(Malloc, CLSID_DxcCompiler, riid,
                                          &Compiler));
    IFR(DxilExtValSupport.CreateInstance2<IDxcValidator>(
        Malloc, CLSID_DxcValidator, &Validator));
  } else {
    IFR(DxCompilerSupport.CreateInstance(CLSID_DxcCompiler, riid, &Compiler));
    IFR(DxilExtValSupport.CreateInstance<IDxcValidator>(CLSID_DxcValidator,
                                                        &Validator));
  }

  // Wrap compiler
  CComPtr<ExternalValidationCompiler> CompilerWrapper =
      ExternalValidationCompiler::Alloc(Malloc ? Malloc
                                               : DxcGetThreadMallocNoRef(),
                                        Validator, riid, Compiler);
  return CompilerWrapper->QueryInterface(riid, (void **)ResultObject);
}

HRESULT DxcDllExtValidationLoader::CreateInstanceImpl(REFCLSID clsid,
                                                      REFIID riid,
                                                      IUnknown **ResultObject) {
  if (!ResultObject)
    return E_POINTER;

  *ResultObject = nullptr;

  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance(clsid, riid, ResultObject);
    }
    if (clsid == CLSID_DxcCompiler) {
      return CreateCompilerWrapper(nullptr, DxCompilerSupport,
                                   DxilExtValSupport, riid, ResultObject);
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance(clsid, riid, ResultObject);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2Impl(
    IMalloc *Malloc, REFCLSID clsid, REFIID riid, IUnknown **ResultObject) {
  if (!ResultObject)
    return E_POINTER;

  *ResultObject = nullptr;
  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !DxilDllFailedToLoad()) {
    if (clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance2(Malloc, clsid, riid,
                                               ResultObject);
    }
    if (clsid == CLSID_DxcCompiler) {
      return CreateCompilerWrapper(Malloc, DxCompilerSupport, DxilExtValSupport,
                                   riid, ResultObject);
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance2(Malloc, clsid, riid, ResultObject);
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
