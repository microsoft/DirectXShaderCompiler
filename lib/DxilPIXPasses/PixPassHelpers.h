///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixPassHelpers.h
// // Copyright (C) Microsoft Corporation. All rights reserved. // This file is
// distributed under the University of Illinois Open Source     // License. See
// LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <climits>
#include <functional>
#include <vector>

#include "dxc/DXIL/DxilModule.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

// #define PIX_DEBUG_DUMP_HELPER
#ifdef PIX_DEBUG_DUMP_HELPER
#include "dxc/Support/Global.h"
#endif

namespace PIXPassHelpers {

class ScopedInstruction {
  llvm::Instruction *m_Instruction;

public:
  ScopedInstruction(llvm::Instruction *I) : m_Instruction(I) {}
  ~ScopedInstruction() { delete m_Instruction; }
  llvm::Instruction *Get() const { return m_Instruction; }
};

void FindRayQueryHandlesForFunction(
    llvm::Function *F, llvm::SmallPtrSetImpl<llvm::Value *> &RayQueryHandles);
enum class PixUAVHandleMode { NonLib, Lib };
llvm::CallInst *CreateUAVOnceForModule(hlsl::DxilModule &DM,
                                       llvm::IRBuilder<> &Builder,
                                       unsigned int hlslBindIndex,
                                       const char *name);
hlsl::DxilResource *CreateGlobalUAVResource(hlsl::DxilModule &DM,
                                            unsigned int hlslBindIndex,
                                            const char *name);
llvm::CallInst *CreateHandleForResource(hlsl::DxilModule &DM,
                                        llvm::IRBuilder<> &Builder,
                                        hlsl::DxilResourceBase *resource,
                                        const char *name);
llvm::Function *GetEntryFunction(hlsl::DxilModule &DM);
void EraseIfUnused(hlsl::DxilModule &DM, llvm::Function *OpFunction);
// A stale ViewID dependency table describes registers that do not match the
// module's current signature sizes. Call after appending a signature
// element.
void ClearViewIdState(hlsl::DxilModule &DM);
std::vector<llvm::Function *>
GetAllInstrumentableFunctions(hlsl::DxilModule &DM);
// Inlines each function that the runtime does not invoke into its callers, and
// erases the inlined-away body.
//
// PIX identifies one shader invocation by one record stream in the debug UAV,
// and maps that stream to exactly one function. A helper instrumented as a
// function of its own therefore reads as a second invocation of a thread that
// runs once, and PIX discards its records. An inlined helper stays visible in
// the inlinedAt chain of the debug locations, which is where PIX looks for it.
//
// Call this before any pass numbers instructions or synthesizes shadow storage.
// PIX steps through the ordinals of the module this leaves behind, and
// llvm::InlineFunction stamps the call site debug location onto each inlined
// instruction that carries none. This function is idempotent, so every pass
// that can come first in a PIX pipeline calls it.
//
// A library module keeps every function, because each exported function is an
// invocation of its own.
//
// UninlinedFunctions, when supplied, receives each non-entry function that is
// still in the module afterwards. Such a function keeps an instruction range
// that no trace record arrives for, so the pass that advertises those ranges
// supplies this parameter and reports what it receives. A pass that advertises
// no range supplies nothing and stays silent, which also keeps one pipeline
// from naming the same function twice.
//
// The survivor set is recomputed on every call, so a caller still receives it
// when an earlier caller already inlined the module.
bool InlineNonEntryFunctions(
    hlsl::DxilModule &DM,
    llvm::SmallVectorImpl<llvm::Function *> *UninlinedFunctions = nullptr);
hlsl::DXIL::ShaderKind GetFunctionShaderKind(hlsl::DxilModule &DM,
                                             llvm::Function *fn);
#ifdef PIX_DEBUG_DUMP_HELPER
void Log(const char *format, ...);
void LogPartialLine(const char *format, ...);
void IncreaseLogIndent();
void DecreaseLogIndent();
void DumpFullType(llvm::DIType const *type);
#else
inline void DumpFullType(llvm::DIType const *) {}
inline void Log(const char *, ...) {}
inline void LogPartialLine(const char *format, ...) {}
inline void IncreaseLogIndent() {}
inline void DecreaseLogIndent() {}
#endif
class ScopedIndenter {
public:
  ScopedIndenter() { IncreaseLogIndent(); }
  ~ScopedIndenter() { DecreaseLogIndent(); }
};

struct ExpandedStruct {
  llvm::Type *ExpandedPayloadStructType = nullptr;
  llvm::Type *ExpandedPayloadStructPtrType = nullptr;
};

ExpandedStruct ExpandStructType(llvm::LLVMContext &Ctx,
                                llvm::Type *OriginalPayloadStructType);
void ReplaceAllUsesOfInstructionWithNewValueAndDeleteInstruction(
    llvm::Instruction *Instr, llvm::Value *newValue, llvm::Type *newType);
// Passed as UpStreamSVPosRow when the caller cannot determine which row the
// previous stage uses for SV_Position. See FindOrAddSV_Position.
constexpr unsigned kUnknownSVPositionRow = UINT_MAX;

// States how much the caller of FindOrAddSV_Position knows about
// UpStreamSVPosRow. The row value alone cannot distinguish the two states, so
// the caller states its confidence explicitly.
enum class SVPositionRowAuthority {
  // The row may not be genuine. SV_Position is placed there only if the row
  // is free; nothing already in the signature moves.
  Hint,
  // The row is the register the previous stage writes SV_Position to.
  // SV_Position lands there, and any occupant is repacked elsewhere.
  Authoritative,
};

// Hint is the default: it cannot make an existing signature worse, because
// nothing already present is moved.
unsigned int FindOrAddSV_Position(
    hlsl::DxilModule &DM, unsigned UpStreamSVPosRow,
    SVPositionRowAuthority RowAuthority = SVPositionRowAuthority::Hint);
void ForEachDynamicallyIndexedResource(
    hlsl::DxilModule &DM,
    const std::function<bool(bool, llvm::Instruction *, llvm::Value *)>
        &Visitor);
} // namespace PIXPassHelpers
