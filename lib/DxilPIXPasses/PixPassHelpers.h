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
