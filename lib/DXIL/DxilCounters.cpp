///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCounters.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilCounters.h"
#include "dxc/Support/Global.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/DenseMap.h"

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilInstructions.h"

using namespace llvm;
using namespace hlsl;
using namespace hlsl::DXIL;

namespace hlsl {

namespace {

struct PointerInfo {
  enum class MemType : unsigned {
    Unknown = 0,
    Global_Static,
    Global_TGSM,
    Alloca
  };

  MemType memType : 2;
  bool isArray : 1;

  PointerInfo() :
    memType(MemType::Unknown),
    isArray(false)
  {}
};

typedef DenseMap<Value*, PointerInfo> PointerInfoMap;

PointerInfo GetPointerInfo(Value* V, PointerInfoMap &ptrInfoMap) {
  auto it = ptrInfoMap.find(V);
  if (it != ptrInfoMap.end())
    return it->second;

  PointerInfo &PI = ptrInfoMap[V];
  Type *Ty = V->getType()->getPointerElementType();
  PI.isArray = Ty->isArrayTy();

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
    if (GV->getType()->getPointerAddressSpace() == DXIL::kTGSMAddrSpace)
      PI.memType = PointerInfo::MemType::Global_TGSM;
    else if (!GV->isConstant() &&
             GV->getLinkage() == GlobalVariable::LinkageTypes::InternalLinkage &&
             GV->getType()->getPointerAddressSpace() == DXIL::kDefaultAddrSpace)
      PI.memType = PointerInfo::MemType::Global_Static;
  } else if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
    PI.memType = PointerInfo::MemType::Alloca;
  } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
    PI = GetPointerInfo(GEP->getPointerOperand(), ptrInfoMap);
  } else if (BitCastOperator *BC = dyn_cast<BitCastOperator>(V)) {
    PI = GetPointerInfo(BC->getOperand(0), ptrInfoMap);
  } else if (AddrSpaceCastInst *AC = dyn_cast<AddrSpaceCastInst>(V)) {
    PI = GetPointerInfo(AC->getOperand(0), ptrInfoMap);
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
    if (CE->getOpcode() == LLVMAddrSpaceCast)
      PI = GetPointerInfo(AC->getOperand(0), ptrInfoMap);
  //} else if (PHINode *PN = dyn_cast<PHINode>(V)) {
  //  for (auto it = PN->value_op_begin(), e = PN->value_op_end(); it != e; ++it) {
  //    PI = GetPointerInfo(*it, ptrInfoMap);
  //    if (PI.memType != PointerInfo::MemType::Unknown)
  //      break;
  //  }
  }
  return PI;
};

struct ValueInfo {
  bool isCbuffer : 1;
  bool isConstant : 1;

  ValueInfo() :
    isCbuffer(false),
    isConstant(false)
  {}

  ValueInfo Combine(const ValueInfo &other) const {
    ValueInfo R;
    R.isCbuffer = isCbuffer && other.isCbuffer;
    R.isConstant = isConstant && other.isConstant;
    return R;
  }
};

typedef SmallDenseMap<Value*, ValueInfo, 16> ValueInfoMap;

ValueInfo GetValueInfo(Value* V, ValueInfoMap &valueInfoMap) {
  auto it = valueInfoMap.find(V);
  if (it != valueInfoMap.end())
    return it->second;

  ValueInfo &VI = valueInfoMap[V];

  if (Constant *C = dyn_cast<Constant>(V)) {
    VI.isConstant = true;
  } else if (CallInst *CI = dyn_cast<CallInst>(V)) {
    if (hlsl::OP::IsDxilOpFuncCallInst(CI)) {
      OpCode opcode = (OpCode)llvm::cast<llvm::ConstantInt>(CI->getOperand(0))->getZExtValue();
      if (opcode == OpCode::CBufferLoad || opcode == OpCode::CBufferLoadLegacy)
        VI.isCbuffer = true;
    }
  } else if (CmpInst *CMP = dyn_cast<CmpInst>(V)) {
    VI = GetValueInfo(CMP->getOperand(0), valueInfoMap).Combine(
         GetValueInfo(CMP->getOperand(1), valueInfoMap));
  } else if (ExtractElementInst *EE = dyn_cast<ExtractElementInst>(V)) {
    VI = GetValueInfo(EE->getVectorOperand(), valueInfoMap);
  }
  // TODO: fill out more as necessary

  return VI;
}

/*<py>

counters = [
  'int', 'uint', 'float',
  'tex_norm', 'tex_load', 'tex_cmp', 'tex_bias', 'tex_grad', 'tex_store',
  'fence', 'atomic', 'barrier',
  'sig',
  'gs_emit', 'gs_cut',
  ]

def tab_lines(text):
  return ['  ' + line for line in text.splitlines()]

def gen_count_dxil_op(counter):
  return (['bool CountDxilOp_%s(unsigned op) {' % counter] +
          tab_lines(
            hctdb_instrhelp.get_instrs_pred("op", hctdb_instrhelp.counter_pred(counter, True))) +
          ['}'])

def gen_count_llvm_op(counter):
  return (['bool CountLlvmOp_%s(unsigned op) {' % counter] +
          tab_lines(
            hctdb_instrhelp.get_instrs_pred("op", hctdb_instrhelp.counter_pred(counter, False), 'llvm_id')) +
          ['}'])

def gen_counter_functions(counters):
  lines = ['// Counter functions for Dxil ops:']
  for counter in counters:
    lines += gen_count_dxil_op(counter)
  lines.append('// Counter functions for llvm ops:')
  for counter in counters:
    lines += gen_count_llvm_op(counter)
  return lines

</py>*/

// <py::lines('OPCODE-COUNTERS')>gen_counter_functions(counters)</py>
// OPCODE-COUNTERS:BEGIN
// Counter functions for Dxil ops:
bool CountDxilOp_int(unsigned op) {
  // Instructions: IMax=37, IMin=38, IMul=41, IMad=48, Ibfe=51,
  // Dot4AddI8Packed=163
  return (37 <= op && op <= 38) || op == 41 || op == 48 || op == 51 || op == 163;
}
bool CountDxilOp_uint(unsigned op) {
  // Instructions: Bfrev=30, Countbits=31, FirstbitLo=32, FirstbitHi=33,
  // FirstbitSHi=34, UMax=39, UMin=40, UMul=42, UDiv=43, UAddc=44, USubb=45,
  // UMad=49, Msad=50, Ubfe=52, Bfi=53, Dot4AddU8Packed=164
  return (30 <= op && op <= 34) || (39 <= op && op <= 40) || (42 <= op && op <= 45) || (49 <= op && op <= 50) || (52 <= op && op <= 53) || op == 164;
}
bool CountDxilOp_float(unsigned op) {
  // Instructions: FAbs=6, Saturate=7, IsNaN=8, IsInf=9, IsFinite=10,
  // IsNormal=11, Cos=12, Sin=13, Tan=14, Acos=15, Asin=16, Atan=17, Hcos=18,
  // Hsin=19, Htan=20, Exp=21, Frc=22, Log=23, Sqrt=24, Rsqrt=25, Round_ne=26,
  // Round_ni=27, Round_pi=28, Round_z=29, FMax=35, FMin=36, Fma=47, Dot2=54,
  // Dot3=55, Dot4=56, Dot2AddHalf=162
  return (6 <= op && op <= 29) || (35 <= op && op <= 36) || op == 47 || (54 <= op && op <= 56) || op == 162;
}
bool CountDxilOp_tex_norm(unsigned op) {
  // Instructions: Sample=60, SampleLevel=62, TextureGather=73
  return op == 60 || op == 62 || op == 73;
}
bool CountDxilOp_tex_load(unsigned op) {
  // Instructions: TextureLoad=66, BufferLoad=68, RawBufferLoad=139
  return op == 66 || op == 68 || op == 139;
}
bool CountDxilOp_tex_cmp(unsigned op) {
  // Instructions: SampleCmp=64, SampleCmpLevelZero=65, TextureGatherCmp=74
  return (64 <= op && op <= 65) || op == 74;
}
bool CountDxilOp_tex_bias(unsigned op) {
  // Instructions: SampleBias=61
  return op == 61;
}
bool CountDxilOp_tex_grad(unsigned op) {
  // Instructions: SampleGrad=63
  return op == 63;
}
bool CountDxilOp_tex_store(unsigned op) {
  // Instructions: TextureStore=67, BufferStore=69, RawBufferStore=140,
  // WriteSamplerFeedback=174, WriteSamplerFeedbackBias=175,
  // WriteSamplerFeedbackLevel=176, WriteSamplerFeedbackGrad=177
  return op == 67 || op == 69 || op == 140 || (174 <= op && op <= 177);
}
bool CountDxilOp_fence(unsigned op) {
  // Instructions:
  return false;
}
bool CountDxilOp_atomic(unsigned op) {
  // Instructions: BufferUpdateCounter=70, AtomicBinOp=78,
  // AtomicCompareExchange=79
  return op == 70 || (78 <= op && op <= 79);
}
bool CountDxilOp_barrier(unsigned op) {
  // Instructions: Barrier=80
  return op == 80;
}
bool CountDxilOp_sig(unsigned op) {
  // Instructions: LoadInput=4, StoreOutput=5, LoadOutputControlPoint=103,
  // LoadPatchConstant=104, StorePatchConstant=106, StoreVertexOutput=171,
  // StorePrimitiveOutput=172
  return (4 <= op && op <= 5) || (103 <= op && op <= 104) || op == 106 || (171 <= op && op <= 172);
}
bool CountDxilOp_gs_emit(unsigned op) {
  // Instructions: EmitStream=97, EmitThenCutStream=99
  return op == 97 || op == 99;
}
bool CountDxilOp_gs_cut(unsigned op) {
  // Instructions: CutStream=98, EmitThenCutStream=99
  return (98 <= op && op <= 99);
}
// Counter functions for llvm ops:
bool CountLlvmOp_int(unsigned op) {
  // Instructions: Add=8, Sub=10, Mul=12, SDiv=15, SRem=18, AShr=22, Trunc=33,
  // SExt=35, ICmp=46
  return op == 8 || op == 10 || op == 12 || op == 15 || op == 18 || op == 22 || op == 33 || op == 35 || op == 46;
}
bool CountLlvmOp_uint(unsigned op) {
  // Instructions: UDiv=14, URem=17, Shl=20, LShr=21, And=23, Or=24, Xor=25,
  // ZExt=34
  return op == 14 || op == 17 || (20 <= op && op <= 21) || (23 <= op && op <= 25) || op == 34;
}
bool CountLlvmOp_float(unsigned op) {
  // Instructions: FAdd=9, FSub=11, FMul=13, FDiv=16, FRem=19, FPToUI=36,
  // FPToSI=37, UIToFP=38, SIToFP=39, FPTrunc=40, FPExt=41, FCmp=47
  return op == 9 || op == 11 || op == 13 || op == 16 || op == 19 || (36 <= op && op <= 41) || op == 47;
}
bool CountLlvmOp_tex_norm(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_tex_load(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_tex_cmp(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_tex_bias(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_tex_grad(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_tex_store(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_fence(unsigned op) {
  // Instructions: Fence=30
  return op == 30;
}
bool CountLlvmOp_atomic(unsigned op) {
  // Instructions: AtomicCmpXchg=31, AtomicRMW=32
  return (31 <= op && op <= 32);
}
bool CountLlvmOp_barrier(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_sig(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_gs_emit(unsigned op) {
  // Instructions:
  return false;
}
bool CountLlvmOp_gs_cut(unsigned op) {
  // Instructions:
  return false;
}
// OPCODE-COUNTERS:END

void CountDxilOp(unsigned op, DxilCounters &counters) {
  if (CountDxilOp_int(op)) ++counters.IntInstructionCount;
  if (CountDxilOp_uint(op)) ++counters.UintInstructionCount;
  if (CountDxilOp_float(op)) ++counters.FloatInstructionCount;

  if (CountDxilOp_tex_norm(op)) ++counters.TextureSampleCount;
  if (CountDxilOp_tex_load(op)) ++counters.ResourceLoadCount;
  if (CountDxilOp_tex_cmp(op)) ++counters.TextureCmpCount;
  if (CountDxilOp_tex_bias(op)) ++counters.TextureBiasCount;
  if (CountDxilOp_tex_grad(op)) ++counters.TextureGradCount;
  if (CountDxilOp_tex_store(op)) ++counters.ResourceStoreCount;

  if (CountDxilOp_barrier(op)) ++counters.BarrierCount;
  if (CountDxilOp_atomic(op)) ++counters.AtomicCount;

  if (CountDxilOp_gs_emit(op)) ++counters.GSEmitCount;
  if (CountDxilOp_gs_cut(op)) ++counters.GSCutCount;
}

void CountLlvmOp(unsigned op, DxilCounters &counters) {
  if (CountLlvmOp_int(op)) ++counters.IntInstructionCount;
  if (CountLlvmOp_uint(op)) ++counters.UintInstructionCount;
  if (CountLlvmOp_float(op)) ++counters.FloatInstructionCount;

  if (CountLlvmOp_tex_norm(op)) ++counters.TextureSampleCount;
  if (CountLlvmOp_tex_load(op)) ++counters.ResourceLoadCount;
  if (CountLlvmOp_tex_cmp(op)) ++counters.TextureCmpCount;
  if (CountLlvmOp_tex_bias(op)) ++counters.TextureBiasCount;
  if (CountLlvmOp_tex_grad(op)) ++counters.TextureGradCount;
  if (CountLlvmOp_tex_store(op)) ++counters.ResourceStoreCount;

  if (CountLlvmOp_barrier(op)) ++counters.BarrierCount;
  if (CountLlvmOp_atomic(op)) ++counters.AtomicCount;

  if (CountLlvmOp_gs_emit(op)) ++counters.GSEmitCount;
  if (CountLlvmOp_gs_cut(op)) ++counters.GSCutCount;
}

} // namespace

void CountInstructions(llvm::Module &M, DxilCounters& counters) {
  const DataLayout &DL = M.getDataLayout();
  PointerInfoMap ptrInfoMap;

  for (auto &GV : M.globals()) {
    PointerInfo PI = GetPointerInfo(&GV, ptrInfoMap);
    if (PI.isArray) {
      // Count number of bytes used in global arrays.
      Type *pTy = GV.getType()->getPointerElementType();
      uint32_t size = DL.getTypeAllocSize(pTy);
      switch (PI.memType) {
      case PointerInfo::MemType::Global_Static:  counters.GlobalStaticArrayBytes += size;  break;
      case PointerInfo::MemType::Global_TGSM:    counters.GlobalTGSMArrayBytes += size;    break;
      default: break;
      }
    }
  }

  for (auto &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    for (auto itBlock = F.begin(), endBlock = F.end(); itBlock != endBlock; ++itBlock) {
      for (auto itInst = itBlock->begin(), endInst = itBlock->end(); itInst != endInst; ++itInst) {
        Instruction* I = itInst;
        ++counters.InstructionCount;
        if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
          Type *pTy = AI->getType()->getPointerElementType();
          // Count number of bytes used in alloca arrays.
          if (pTy->isArrayTy()) {
            counters.LocalArrayBytes += DL.getTypeAllocSize(pTy);
          }
        } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
          if (hlsl::OP::IsDxilOpFuncCallInst(CI)) {
            unsigned opcode = (unsigned)llvm::cast<llvm::ConstantInt>(I->getOperand(0))->getZExtValue();
            CountDxilOp(opcode, counters);
          }
        } else if (isa<LoadInst>(I) || isa<StoreInst>(I)) {
          LoadInst  *LI = dyn_cast<LoadInst>(I);
          StoreInst *SI = dyn_cast<StoreInst>(I);
          Value *PtrOp = LI ? LI->getPointerOperand() : SI->getPointerOperand();
          PointerInfo PI = GetPointerInfo(PtrOp, ptrInfoMap);
          // Count load/store on array elements.
          if (PI.isArray) {
            switch (PI.memType) {
            case PointerInfo::MemType::Alloca:         ++counters.LocalArrayAccesses;        break;
            case PointerInfo::MemType::Global_Static:  ++counters.GlobalStaticArrayAccesses; break;
            case PointerInfo::MemType::Global_TGSM:    ++counters.GlobalTGSMArrayAccesses;   break;
            default: break;
            }
          }
        } else if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
          if (BI->getNumSuccessors() > 1) {
            // TODO: More sophisticated analysis to separate dynamic from static branching?
            ++counters.BranchCount;
          }
        } else {
          // Count llvm ops:
          CountLlvmOp(I->getOpcode(), counters);
        }
      }
    }
  }
}

struct CounterOffsetByName {
  StringRef name;
  uint32_t DxilCounters::*ptr;
};

static const CounterOffsetByName CountersByName[] = {
#define DECLARE_MEMBER(name) { #name, &DxilCounters::name }
  // Must be sorted case-sensitive:
  DECLARE_MEMBER(AtomicCount),
  DECLARE_MEMBER(BarrierCount),
  DECLARE_MEMBER(BranchCount),
  DECLARE_MEMBER(FloatInstructionCount),
  DECLARE_MEMBER(GSCutCount),
  DECLARE_MEMBER(GSEmitCount),
  DECLARE_MEMBER(GlobalStaticArrayAccesses),
  DECLARE_MEMBER(GlobalStaticArrayBytes),
  DECLARE_MEMBER(GlobalTGSMArrayAccesses),
  DECLARE_MEMBER(GlobalTGSMArrayBytes),
  DECLARE_MEMBER(InstructionCount),
  DECLARE_MEMBER(IntInstructionCount),
  DECLARE_MEMBER(LocalArrayAccesses),
  DECLARE_MEMBER(LocalArrayBytes),
  DECLARE_MEMBER(ResourceLoadCount),
  DECLARE_MEMBER(ResourceStoreCount),
  DECLARE_MEMBER(TextureBiasCount),
  DECLARE_MEMBER(TextureCmpCount),
  DECLARE_MEMBER(TextureGradCount),
  DECLARE_MEMBER(TextureSampleCount),
  DECLARE_MEMBER(UintInstructionCount)
#undef DECLARE_MEMBER
};

static int CounterOffsetByNameLess(const CounterOffsetByName &a, const CounterOffsetByName &b) {
  return a.name < b.name;
}

uint32_t *LookupByName(llvm::StringRef name, DxilCounters& counters) {
  CounterOffsetByName key = {name, nullptr};
  static const CounterOffsetByName *CounterEnd = CountersByName +_countof(CountersByName);
  auto result = std::lower_bound(CountersByName, CounterEnd, key, CounterOffsetByNameLess);
  if (result != CounterEnd && result->name == key.name)
    return &(counters.*(result->ptr));
  return nullptr;
}


} // namespace hlsl
