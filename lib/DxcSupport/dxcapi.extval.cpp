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
  std::vector<std::wstring> ArgStorage;
  llvm::SmallVector<LPCWSTR, 16> NewArgs;
  hlsl::options::DxcOpts Opts;
  CComPtr<IDxcValidator> Validator;
  UINT32 ValidatorVersionMajor = 0;
  UINT32 ValidatorVersionMinor = 0;
  bool NeedsValidation = false;
  bool RootSignatureOnly = false;

public:
  ExtValidationArgHelper() = default;
  ExtValidationArgHelper(const ExtValidationArgHelper &) = delete;
  HRESULT initialize(IDxcValidator *NewValidator, LPCWSTR **Arguments,
                     UINT32 *ArgCount, LPCWSTR TargetProfile = nullptr) {
    IFR(setValidator(NewValidator));
    IFR(processArgs(Arguments, ArgCount, TargetProfile));
    return S_OK;
  }

  HRESULT doValidation(IDxcOperationResult *CompileResult, REFIID Riid,
                       void **ValResult) {
    // this lambda takes an arbitrary object that implements the
    // IDxcOperationResult interface, asks whether that object
    // also implements the interface specified by Riid, and if
    // so, sets ValResult to point to that object
    auto UseResult = [&](IDxcOperationResult *Result) -> HRESULT {
      if (Result)
        return Result->QueryInterface(Riid, ValResult);
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
    if (FAILED(HR)) {
      // prefix the stderr output
      fprintf(stderr , "error: validation errors\r\n");
      return UseResult(TempValidationResult);
    }

    // Validation succeeded. Return the original compile result.
    return UseResult(CompileResult);
  }

private:
  /// Add argument to ArgStorage to be referenced by NewArgs.
  void addArgument(const std::wstring &Arg) { ArgStorage.push_back(Arg); }

  HRESULT setValidator(IDxcValidator *NewValidator) {
    Validator = NewValidator;
    DXASSERT(Validator, "Invalid Validator argument");

    // 1.0 had no version interface.
    ValidatorVersionMajor = 1;
    ValidatorVersionMinor = 0;
    CComPtr<IDxcVersionInfo> ValidatorVersionInfo;
    if (SUCCEEDED(
            Validator->QueryInterface(IID_PPV_ARGS(&ValidatorVersionInfo)))) {
      if (ValidatorVersionInfo->GetVersion(&ValidatorVersionMajor,
                                           &ValidatorVersionMinor)) {
        DXASSERT(false, "Failed to get validator version");
        return E_FAIL;
      }
    }
    return S_OK;
  }

  /// Process arguments, adding validator version and disable validation if
  /// needed. If TargetProfile provided, use this instead of -T argument.
  HRESULT processArgs(LPCWSTR **Arguments, UINT32 *ArgCount,
                      LPCWSTR TargetProfile = nullptr) {
    DXASSERT((Arguments && ArgCount) && (*Arguments || *ArgCount == 0) &&
                 *ArgCount < INT_MAX - 3,
             "Invalid Arguments and ArgCount arguments");
    NeedsValidation = false;

    // Must not call twice.
    IFRBOOL(NewArgs.empty(), E_FAIL);

    // Parse compiler arguments to Opts.
    std::string Errors;
    llvm::raw_string_ostream OS(Errors);
    hlsl::options::MainArgs MainArgs(static_cast<int>(*ArgCount), *Arguments,
                                     0);
    if (hlsl::options::ReadDxcOpts(hlsl::options::getHlslOptTable(),
                                   hlsl::options::CompilerFlags, MainArgs, Opts,
                                   OS))
      return E_FAIL;

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
        addArgument(L"-validator-version");
        std::wstring VerStr = std::to_wstring(ValidatorVersionMajor) + L"." +
                              std::to_wstring(ValidatorVersionMinor);
        addArgument(VerStr);
      }

      // Add argument to disable validation, so we can call it ourselves
      // later.
      if (NeedsValidation)
        addArgument(L"-Vd");
    }

    if (!ArgStorage.size())
      return S_OK;

    // Reference added arguments from storage.
    NewArgs.reserve(*ArgCount + ArgStorage.size());

    // Copied arguments refer to original strings.
    for (UINT32 i = 0; i < *ArgCount; ++i)
      NewArgs.push_back((*Arguments)[i]);

    for (const auto &Arg : ArgStorage)
      NewArgs.push_back(Arg.c_str());

    *Arguments = NewArgs.data();
    *ArgCount = (UINT32)NewArgs.size();
    return S_OK;
  }
};

// ExternalValidationCompiler provides a DxCompiler that performs validation
// using an external validator instead of an internal one.
// It uses a wrapping approach, where it wraps the provided compiler object,
// adds '-Vd' and '-validator-version' when appropriate to skip internal
// validation and set the default validator version, then uses the provided
// IDxcValidator interface to validate a successful compilation result, when
// validation would normally be performed.
class ExternalValidationCompiler : public IDxcCompiler2, public IDxcCompiler3 {
private:
  CComPtr<IDxcValidator> Validator;

  // This wrapper wraps one particular compiler interface.
  // When QueryInterface is called, we create a new wrapper
  // for the requested interface, which wraps the result of QueryInterface
  // on the compiler object. Compiler pointer is held as IUnknown and must
  // be upcast to the appropriate interface on use.
  IID CompilerIID;
  CComPtr<IUnknown> Compiler;

  // Cast current compiler interface pointer. Used from methods of the
  // associated interface, assuming that the current compiler interface is
  // correct for the method call.
  // This will either be casting to the original interface retrieved by
  // QueryInterface, or to one from which that interface derives.
  template <typename T> T *castCompilerSafe() const {
    // Compare stored IID with the IID of T
    if (CompilerIID == __uuidof(T)) {
      // Safe to cast because the underlying compiler object in
      // Compiler originally implemented the interface T
      return static_cast<T *>(Compiler.p);
    }

    return nullptr;
  }

  template <typename T> T *castCompilerUnsafe() {
    if (T *Safe = castCompilerSafe<T>())
      return Safe;
    return static_cast<T *>(Compiler.p);
  }

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
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID Iid,
                                           void **ResultObject) override {
    if (!ResultObject)
      return E_POINTER;

    *ResultObject = nullptr;

    // Get pointer for an interface the compiler object supports.
    CComPtr<IUnknown> TempCompiler;
    IFR(Compiler->QueryInterface(Iid, (void **)&TempCompiler));

    try {
      // This can throw; do not leak C++ exceptions through COM method
      // calls.
      CComPtr<ExternalValidationCompiler> NewWrapper(
          Alloc(m_pMalloc, Validator, Iid, TempCompiler));
      return DoBasicQueryInterface<IDxcCompiler, IDxcCompiler2, IDxcCompiler3>(
          NewWrapper.p, Iid, ResultObject);
    } catch (...) {
      return E_FAIL;
    }
  }

  // IDxcCompiler implementation
  HRESULT STDMETHODCALLTYPE
  Compile(IDxcBlob *Source, LPCWSTR SourceName, LPCWSTR EntryPoint,
          LPCWSTR TargetProfile, LPCWSTR *Arguments, UINT32 ArgCount,
          const DxcDefine *Defines, UINT32 DefineCount,
          IDxcIncludeHandler *IncludeHandler,
          IDxcOperationResult **ResultObject) override {
    if (ResultObject == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    // initialize will update Arguments and ArgCount if needed.
    ExtValidationArgHelper Helper;
    IFR(Helper.initialize(Validator, &Arguments, &ArgCount, TargetProfile));

    CComPtr<IDxcOperationResult> CompileResult;
    IFR(castCompilerUnsafe<IDxcCompiler>()->Compile(
        Source, SourceName, EntryPoint, TargetProfile, Arguments, ArgCount,
        Defines, DefineCount, IncludeHandler, &CompileResult));
    HRESULT CompileHR;
    CompileResult->GetStatus(&CompileHR);
    if (SUCCEEDED(CompileHR))
        return Helper.doValidation(CompileResult, IID_PPV_ARGS(ResultObject));
    CompileResult->QueryInterface(ResultObject);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  Preprocess(IDxcBlob *Source, LPCWSTR SourceName, LPCWSTR *Arguments,
             UINT32 ArgCount, const DxcDefine *Defines, UINT32 DefineCount,
             IDxcIncludeHandler *IncludeHandler,
             IDxcOperationResult **ResultObject) override {
    return castCompilerUnsafe<IDxcCompiler>()->Preprocess(
        Source, SourceName, Arguments, ArgCount, Defines, DefineCount,
        IncludeHandler, ResultObject);
  }

  HRESULT STDMETHODCALLTYPE
  Disassemble(IDxcBlob *Source, IDxcBlobEncoding **Disassembly) override {
    return castCompilerUnsafe<IDxcCompiler>()->Disassemble(Source, Disassembly);
  }

  // IDxcCompiler2 implementation
  HRESULT STDMETHODCALLTYPE CompileWithDebug(
      IDxcBlob *Source, LPCWSTR SourceName, LPCWSTR EntryPoint,
      LPCWSTR TargetProfile, LPCWSTR *Arguments, UINT32 ArgCount,
      const DxcDefine *pDefines, UINT32 DefineCount,
      IDxcIncludeHandler *IncludeHandler, IDxcOperationResult **ResultObject,
      LPWSTR *DebugBlobName, IDxcBlob **DebugBlob) override {
    if (ResultObject == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    // ProcessArgs will update Arguments and ArgCount if needed.
    ExtValidationArgHelper Helper;

    IFR(Helper.initialize(Validator, &Arguments, &ArgCount, TargetProfile));

    CComPtr<IDxcOperationResult> CompileResult;
    IFR(castCompilerUnsafe<IDxcCompiler2>()->CompileWithDebug(
        Source, SourceName, EntryPoint, TargetProfile, Arguments, ArgCount,
        pDefines, DefineCount, IncludeHandler, &CompileResult, DebugBlobName,
        DebugBlob));

    return Helper.doValidation(CompileResult, IID_PPV_ARGS(ResultObject));
  }

  // IDxcCompiler3 implementation
  HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer *Source, LPCWSTR *Arguments,
                                    UINT32 ArgCount,
                                    IDxcIncludeHandler *IncludeHandler,
                                    REFIID Riid,
                                    LPVOID *ResultObject) override {
    if (ResultObject == nullptr)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    // ProcessArgs will update Arguments and ArgCount if needed.
    ExtValidationArgHelper Helper;

    Helper.initialize(Validator, &Arguments, &ArgCount);

    CComPtr<IDxcResult> CompileResult;
    IFR(castCompilerUnsafe<IDxcCompiler3>()->Compile(
        Source, Arguments, ArgCount, IncludeHandler,
        IID_PPV_ARGS(&CompileResult)));

    return Helper.doValidation(CompileResult, Riid, ResultObject);
  }

  HRESULT STDMETHODCALLTYPE Disassemble(const DxcBuffer *Object, REFIID Riid,
                                        LPVOID *ResultObject) override {
    return castCompilerUnsafe<IDxcCompiler3>()->Disassemble(Object, Riid,
                                                            ResultObject);
  }
};
} // namespace

namespace dxc {

static HRESULT createCompilerWrapper(IMalloc *Malloc,
                                     SpecificDllLoader &DxCompilerSupport,
                                     SpecificDllLoader &DxilExtValSupport,
                                     REFIID Riid, IUnknown **ResultObject) {
  DXASSERT(ResultObject, "Invalid ResultObject");
  *ResultObject = nullptr;

  CComPtr<IUnknown> Compiler;
  CComPtr<IDxcValidator> Validator;

  // Create compiler and validator
  if (Malloc) {
    IFR(DxCompilerSupport.CreateInstance2(Malloc, CLSID_DxcCompiler, Riid,
                                          &Compiler));
    IFR(DxilExtValSupport.CreateInstance2<IDxcValidator>(
        Malloc, CLSID_DxcValidator, &Validator));
  } else {
    IFR(DxCompilerSupport.CreateInstance(CLSID_DxcCompiler, Riid, &Compiler));
    IFR(DxilExtValSupport.CreateInstance<IDxcValidator>(CLSID_DxcValidator,
                                                        &Validator));
  }

  // Wrap compiler
  CComPtr<ExternalValidationCompiler> CompilerWrapper =
      ExternalValidationCompiler::Alloc(Malloc ? Malloc
                                               : DxcGetThreadMallocNoRef(),
                                        Validator, Riid, Compiler);
  return CompilerWrapper->QueryInterface(Riid, (void **)ResultObject);
}

HRESULT DxcDllExtValidationLoader::CreateInstanceImpl(REFCLSID Clsid,
                                                      REFIID Riid,
                                                      IUnknown **ResultObject) {
  if (!ResultObject)
    return E_POINTER;

  *ResultObject = nullptr;

  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !dxilDllFailedToLoad()) {
    if (Clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance(Clsid, Riid, ResultObject);
    }
    if (Clsid == CLSID_DxcCompiler) {
      return createCompilerWrapper(nullptr, DxCompilerSupport,
                                   DxilExtValSupport, Riid, ResultObject);
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance(Clsid, Riid, ResultObject);
}

HRESULT DxcDllExtValidationLoader::CreateInstance2Impl(
    IMalloc *Malloc, REFCLSID Clsid, REFIID Riid, IUnknown **ResultObject) {
  if (!ResultObject)
    return E_POINTER;

  *ResultObject = nullptr;
  // If there is intent to use an external dxil.dll
  if (!DxilDllPath.empty() && !dxilDllFailedToLoad()) {
    if (Clsid == CLSID_DxcValidator) {
      return DxilExtValSupport.CreateInstance2(Malloc, Clsid, Riid,
                                               ResultObject);
    }
    if (Clsid == CLSID_DxcCompiler) {
      return createCompilerWrapper(Malloc, DxCompilerSupport, DxilExtValSupport,
                                   Riid, ResultObject);
    }
  }

  // Fallback: let DxCompiler handle it
  return DxCompilerSupport.CreateInstance2(Malloc, Clsid, Riid, ResultObject);
}

HRESULT
DxcDllExtValidationLoader::initialize(llvm::raw_string_ostream &log) {
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
