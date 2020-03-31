

#pragma once

#include "clang/Basic/SourceLocation.h"

#include "llvm/ADT/StringMap.h"

#include "dxc/DXIL/DxilCBuffer.h"

#include <memory>
#include <vector>

namespace clang {
class HLSLPatchConstantFuncAttr;
namespace CodeGen {
class CodeGenModule;
}
}

namespace llvm {
class Function;
class Module;
class Value;
class DebugLoc;
class Constant;
class GlobalVariable;
class CallInst;
template <typename T, unsigned N> class SmallVector;
}

namespace hlsl {
class HLModule;
struct DxilResourceProperties;
struct DxilFunctionProps;
class DxilFieldAnnotation;
enum class IntrinsicOp;
namespace dxilutil {
class ExportMap;
}
}

namespace CGHLSLMSHelper {

struct EntryFunctionInfo {
  clang::SourceLocation SL = clang::SourceLocation();
  llvm::Function *Func = nullptr;
};

  // Map to save patch constant functions
struct PatchConstantInfo {
  clang::SourceLocation SL = clang::SourceLocation();
  llvm::Function *Func = nullptr;
  std::uint32_t NumOverloads = 0;
};

/// Use this class to represent HLSL cbuffer in high-level DXIL.
class HLCBuffer : public hlsl::DxilCBuffer {
public:
  HLCBuffer() = default;
  virtual ~HLCBuffer() = default;

  void AddConst(std::unique_ptr<DxilResourceBase> &pItem) {
    pItem->SetID(constants.size());
    constants.push_back(std::move(pItem));
  }

  std::vector<std::unique_ptr<DxilResourceBase>> &GetConstants() {
    return constants;
  }

private:
  std::vector<std::unique_ptr<DxilResourceBase>>
      constants; // constants inside const buffer
};

// Align cbuffer offset in legacy mode (16 bytes per row).
unsigned AlignBufferOffsetInLegacy(unsigned offset, unsigned size,
                                   unsigned scalarSizeInBytes,
                                   bool bNeedNewRow);

void FinishEntries(hlsl::HLModule &HLM, const EntryFunctionInfo &Entry,
                   clang::CodeGen::CodeGenModule &CGM,
                   llvm::StringMap<EntryFunctionInfo> &entryFunctionMap,
                   std::unordered_map<llvm::Function *,
                                      const clang::HLSLPatchConstantFuncAttr *>
                       &HSEntryPatchConstantFuncAttr,
                   llvm::StringMap<PatchConstantInfo> &patchConstantFunctionMap,
                   std::unordered_map<llvm::Function *,
                                      std::unique_ptr<hlsl::DxilFunctionProps>>
                       &patchConstantFunctionPropsMap);

void FinishIntrinsics(
    hlsl::HLModule &HLM, std::vector<std::pair<llvm::Function *, unsigned>> &intrinsicMap,
    llvm::DenseMap<llvm::Value *, hlsl::DxilResourceProperties>
        &valToResPropertiesMap);

void AddDxBreak(llvm::Module &M, llvm::SmallVector<llvm::BranchInst*, 16> DxBreaks);

void ReplaceConstStaticGlobals(
    std::unordered_map<llvm::GlobalVariable *, std::vector<llvm::Constant *>>
        &staticConstGlobalInitListMap,
    std::unordered_map<llvm::GlobalVariable *, llvm::Function *> &staticConstGlobalCtorMap);

void FinishClipPlane(hlsl::HLModule &HLM, std::vector<llvm::Function *> &clipPlaneFuncList,
                    std::unordered_map<llvm::Value *, llvm::DebugLoc> &debugInfoMap,
                    clang::CodeGen::CodeGenModule &CGM);

void AddRegBindingsForResourceInConstantBuffer(
    hlsl::HLModule &HLM,
    llvm::DenseMap<llvm::Constant *,
                   llvm::SmallVector<std::pair<hlsl::DXIL::ResourceClass, unsigned>,
                                     1>> &constantRegBindingMap);

void FinishCBuffer(
    hlsl::HLModule &HLM, llvm::Type *CBufferType,
    std::unordered_map<llvm::Constant *, hlsl::DxilFieldAnnotation>
        &AnnotationMap);

void ProcessCtorFunctions(llvm::Module &M, llvm::StringRef globalName,
                          llvm::Instruction *InsertPt);

void TranslateRayQueryConstructor(hlsl::HLModule &HLM);

void UpdateLinkage(
    hlsl::HLModule &HLM, clang::CodeGen::CodeGenModule &CGM,
    hlsl::dxilutil::ExportMap &exportMap,
    llvm::StringMap<EntryFunctionInfo> &entryFunctionMap,
    llvm::StringMap<PatchConstantInfo> &patchConstantFunctionMap);

llvm::Value *TryEvalIntrinsic(llvm::CallInst *CI, hlsl::IntrinsicOp intriOp);
void SimpleTransformForHLDXIR(llvm::Module *pM);
void ExtensionCodeGen(hlsl::HLModule &HLM, clang::CodeGen::CodeGenModule &CGM);
} // namespace CGHLSLMSHelper
