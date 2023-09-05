//===--- PervertexInputVisitor.cpp ---- PerVertex Input Visitor --*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PervertexInputVisitor.h"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvType.h"

#include <stack>

namespace clang {
namespace spirv {

///< After expanding inputs, location need reassignment.
void PervertexInputVisitor::reassignInputsLocation() {
  // kMaxLoc == 64
  uint32_t maxLoc = 64;
  llvm::SmallBitVector usedLocs(maxLoc * 3);
  std::vector<SpirvDecoration *> decorations;
  for (auto i : m_inputDecorationMap) {
    decorations.push_back(i.getSecond());
  }
  std::stable_sort(decorations.begin(), decorations.end(),
                   [](const SpirvDecoration *a, const SpirvDecoration *b) {
                     return a->getParams()[0] < b->getParams()[0];
                   });
  uint32_t nextLocation = 0;
  // Reassign location and order them.
  for (auto dec : decorations) {
    auto location = dec->getParams()[0];
    auto locationCount = spirvBuilder.getStgInputLocationInfo(location);
    dec->setParam(nextLocation, 0);
    while (usedLocs[nextLocation])
      nextLocation++;
    for (uint32_t i = 0; i < locationCount; ++i) {
      usedLocs.set(nextLocation++);
    }
  }
}

///< For expanded variables, we need to decide where to add an extra index zero
///< for SpirvAccessChain and SpirvCompositeExtract. This comes to two access
///< cases : 1. array element. 2. vector channel.
int PervertexInputVisitor::appendIndexZeroAt(QualType baseType,
    llvm::ArrayRef<uint32_t> instIndexes) {
  if (instIndexes.size() > 0 && baseType.getTypePtr()->isStructureType()) {
    /// Access Chain indexes
    uint32_t idx = instIndexes[0];
    for (auto *f : baseType->getAs<RecordType>()->getDecl()->fields()) {
      if (idx == 0) {
        return 1 + appendIndexZeroAt(f->getType(), instIndexes.slice(1));
        break;
      }
      idx--;
    }
  }
  return 0;
}

///< Expand nointerpolation decorated variables/parameters.
///< If a variable/parameter is passed from a decorated inputs, it should be
///< treated as nointerpolated too.
bool PervertexInputVisitor::expandNointerpVarAndParam(SpirvInstruction *spvInst) {
  QualType type = spvInst->getAstResultType();
  bool isExpanded = false;
  auto typePtr = type.getTypePtr();
  if (typePtr->isStructureType()) {
    /// Structure
    isExpanded = expandNointerpStructure(type, spvInst->isNoninterpolated());
  } else if (spvInst->isNoninterpolated()) {
    /// Normal type
    isExpanded = true;
    type =
        astContext.getConstantArrayType(type, llvm::APInt(32, 3), clang::ArrayType::Normal, 0);
    spvInst->setAstResultType(type);
    /// Internal stg input variables won't be a structure.
    /// Only expanded locations would be recorded, will reset them after entry pt wrapper finish.
    if (spvInst->getStorageClass() == spv::StorageClass::Input){
      auto dec = m_inputDecorationMap.lookup(spvInst);
      if (dec != nullptr) {
        auto location = dec->getParams()[0];
        auto locationCount = 3 * spirvBuilder.getStgInputLocationInfo(location);
        spirvBuilder.setStgInputLocationInfo(location, locationCount);
      }
    }
  }
  return isExpanded;
}

bool PervertexInputVisitor::expandNointerpStructure(QualType type,
                                                    bool isVarDecoratedInterp) {
  QualType currentType = type;
  auto typePtr = type.getTypePtr();
  if (m_expandedStructureType.count(typePtr) > 0)
    return true;
  bool hasNoInterp = false;
  if (typePtr->isStructureType()) {
    const auto *structDecl = type->getAs<RecordType>()->getDecl();
    bool structTypeNoInterp = structDecl->hasAttr<HLSLNoInterpolationAttr>();
    uint32_t i = 0;
    for (auto *field : structDecl->fields()) {
      bool expandElem = (isVarDecoratedInterp || structTypeNoInterp ||
                         field->hasAttr<HLSLNoInterpolationAttr>());
      if (field->getType().getTypePtr()->isStructureType())
        expandNointerpStructure(field->getType(), expandElem);
      else if (expandElem) {
        currentType = astContext.getConstantArrayType(
            field->getType(), llvm::APInt(32, 3), clang::ArrayType::Normal, 0);
        field->setType(currentType);
        hasNoInterp = true;
      }
      i++;
    }
  }
  if (hasNoInterp)
    m_expandedStructureType.insert(typePtr);
  return hasNoInterp;
}

///< Get mapped operand used to replace original operand, if not exists, return
///< itself.
SpirvInstruction *
PervertexInputVisitor::getMappedReplaceInstr(SpirvInstruction *i) {
  auto *replacedInstr = m_instrReplaceMap.lookup(i);
  if (replacedInstr)
    return replacedInstr;
  else
    return i;
}

///< For bool type input, add extra NE and related ops to satisfy workaround in
///< HLSL-SPIRV.
SpirvInstruction *
PervertexInputVisitor::getNEInstrForStgLoading(SpirvInstruction *instr) {
  auto instType = instr->getAstResultType();
  /// dst bool type
  QualType elemType = {};
  uint32_t vecSize = 1;
  auto resultBoolType = instType;
  if (resultBoolType->isPointerType())
    resultBoolType = resultBoolType->getPointeeType();
  if (isVectorType(resultBoolType, &elemType, &vecSize)) {
    resultBoolType = astContext.getExtVectorType(astContext.BoolTy, vecSize);
  }
  if (vecSize == 1) {
    elemType = instType;
    resultBoolType = astContext.BoolTy;
  }
  /// zeroVal
  SpirvConstant *zeroVal = nullptr;
  /// spv NE op
  spv::Op op = spv::Op::OpUndef;
  if (isBoolOrVecMatOfBoolType(instType)) {
    op = spv::Op::OpLogicalNotEqual;
    zeroVal = spirvBuilder.getConstantBool(false);
  } else if (isSintOrVecMatOfSintType(instType) ||
             isUintOrVecMatOfUintType(instType)) {
    op = spv::Op::OpINotEqual;
    zeroVal = spirvBuilder.getConstantInt(elemType, llvm::APInt(32, 0));
  } else if (isFloatOrVecMatOfFloatType(instType)) {
    op = spv::Op::OpFOrdNotEqual;
    zeroVal = spirvBuilder.getConstantFloat(elemType, llvm::APFloat(0.0f));
  }
  if (vecSize > 1) {
    llvm::SmallVector<SpirvConstant *, 4> elements(size_t(vecSize), zeroVal);
    zeroVal = spirvBuilder.getConstantComposite(
        astContext.getExtVectorType(elemType, vecSize), elements);
  }

  auto *NEBinOp = new (context)
      SpirvBinaryOp(op, resultBoolType, instr->getSourceLocation(), instr,
                    zeroVal, instr->getSourceRange());
  NEBinOp->setRValue();
  currentFunc->addToInstructionCache(NEBinOp);
  return NEBinOp;
}

///< Add temp function variables, for operand replacement. An original usage to
///< a nointerpolated variable/parameter should be treated as an access to
///< its first element after expanding (data at first provoking vertex).
SpirvInstruction *
PervertexInputVisitor::createFirstPerVertexVar(SpirvInstruction *base,
                                               llvm::StringRef varName) {
  auto loc = base->getSourceLocation();
  auto *vtx = addFunctionTempVar(varName.str(), base->getAstResultType(), loc,
                                 base->isPrecise());
  createVertexStore(vtx, createVertexLoad(base));
  return vtx;
}

SpirvInstruction *
PervertexInputVisitor::createProvokingVertexAccessChain(SpirvInstruction *base,
                                                        uint32_t index,
                                                        QualType resultType) {
  auto loc = base->getSourceLocation();
  auto range = base->getSourceRange();
  llvm::SmallVector<SpirvInstruction *, 1> indexes;
  indexes.push_back(spirvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                                llvm::APInt(32, index)));
  SpirvInstruction *instruction =
      new (context) SpirvAccessChain(resultType, loc, base, indexes, range);
  instruction->setStorageClass(spv::StorageClass::Function);
  instruction->setLayoutRule(base->getLayoutRule());
  instruction->setContainsAliasComponent(base->containsAliasComponent());
  instruction->setNoninterpolated(false);
  currentFunc->addToInstructionCache(instruction);
  return instruction;
}

SpirvVariable *
PervertexInputVisitor::addFunctionTempVar(llvm::StringRef varName,
                                          QualType valueType,
                                          SourceLocation loc, bool isPrecise) {
  QualType varType = valueType;
  if (varType.getTypePtr()->isPointerType())
    varType = varType.getTypePtr()->getPointeeType();
  SpirvVariable *var = new (context) SpirvVariable(
      varType, loc, spv::StorageClass::Function, isPrecise, false, nullptr);
  var->setDebugName(varName);
  currentFunc->addVariable(var);
  return var;
}

///< When use temp variables within a function, we need to add load/store ops.
///< TIP: A nointerpolated input or function parameter will be treated as
///< input.vtx0
///<      within current function, but would be treated as an array will pass to
///<      a function call.
SpirvInstruction *PervertexInputVisitor::createVertexLoad(SpirvInstruction *base) {

  SpirvInstruction *loadPtr =
      new (context) SpirvLoad(base->getAstResultType(), base->getSourceLocation(),
                              base, base->getSourceRange());
  loadPtr->setStorageClass(spv::StorageClass::Function);
  loadPtr->setLayoutRule(base->getLayoutRule());
  loadPtr->setRValue(true);
  currentFunc->addToInstructionCache(loadPtr);
  return loadPtr;
}

void PervertexInputVisitor::createVertexStore(SpirvInstruction *pt,
                                              SpirvInstruction *obj) {
  auto *storeInstr =
      new (context) SpirvStore(pt->getSourceLocation(), pt, obj);
  currentFunc->addToInstructionCache(storeInstr);
}

// Don't add extra index to a simple vector/matrix elem access when base is not
// expanded. Like:
//        %99 = OpAccessChain %_ptr_Function_v3bool %input %int_2 %uint_0
//       %101 = OpAccessChain % _ptr_Function_bool % 99 % int_1
bool PervertexInputVisitor::isNotExpandedVectorAccess(QualType baseType,
                                                      QualType resultType) {
  QualType elemType = {};
  return (isVectorType(baseType, &elemType) ||
          isMxNMatrix(baseType, &elemType)) &&
         elemType == resultType && !baseType.getTypePtr()->isArrayType();
}

///< Visit functions.
///< Most are defined in header file, here's some special process related to
///< type and temp (provoking vertex) variables.
bool PervertexInputVisitor::visit(SpirvDecoration *d) {
  if (d->getDecoration() == spv::Decoration::Location &&
      d->getTarget()->getStorageClass() == spv::StorageClass::Input){
    m_inputDecorationMap[d->getTarget()] = d;
  }
  return true;
}
bool PervertexInputVisitor::visit(SpirvModule *m, Phase phase) {
  if (!m->isPerVertexInterpMode())
    return false;
  currentMod = m;
  return true;
}

bool PervertexInputVisitor::visit(SpirvEntryPoint *ep) {
  // Add var mapping here. First function would be main.
  currentFunc = ep->getEntryPoint();
  /// Refine stg IO variables
  for (auto *var : currentMod->getVariables()) {
    if (!var->isNoninterpolated() || var->getStorageClass() != spv::StorageClass::Input)
      continue;
    auto *stgIOLoadPtr = spirvBuilder.getPerVertexStgInput(var);
    auto varType = var->getAstResultType();
    bool isBool = isa<SpirvBinaryOp>(stgIOLoadPtr);
    if (!isBool) {
      stgIOLoadPtr->setAstResultType(varType);
      continue;
    }

    /// Bool
    auto loc = stgIOLoadPtr->getSourceLocation();
    auto *vtx0 = createProvokingVertexAccessChain(var, 0, varType);
    auto *vtx1 = createProvokingVertexAccessChain(var, 1, varType);
    auto *vtx2 = createProvokingVertexAccessChain(var, 2, varType);
    SpirvInstruction* loadVtx0 =
        dyn_cast<SpirvBinaryOp>(stgIOLoadPtr)->getOperand1();
    dyn_cast<SpirvLoad>(loadVtx0)->setPointer(vtx0);

    vtx0 = getNEInstrForStgLoading(createVertexLoad(vtx0));
    vtx1 = getNEInstrForStgLoading(createVertexLoad(vtx1));
    vtx2 = getNEInstrForStgLoading(createVertexLoad(vtx2));

    /// A arr temp to replace bool array
    auto retType =
        astContext.getConstantArrayType(stgIOLoadPtr->getAstResultType(),
                                        llvm::APInt(32, 3), clang::ArrayType::Normal, 0);
    auto varName = var->getDebugName().str() + "_BoolArr_PerVertex";
    auto *temp = addFunctionTempVar(varName, retType, loc, stgIOLoadPtr->isPrecise());

    auto *tempvtx0 =
        createProvokingVertexAccessChain(temp, 0, stgIOLoadPtr->getAstResultType());
    auto *tempvtx1 =
        createProvokingVertexAccessChain(temp, 1, stgIOLoadPtr->getAstResultType());
    auto *tempvtx2 =
        createProvokingVertexAccessChain(temp, 2, stgIOLoadPtr->getAstResultType());

    /// Store back
    createVertexStore(tempvtx0, vtx0);
    createVertexStore(tempvtx1, vtx1);
    createVertexStore(tempvtx2, vtx2);

    m_instrReplaceMap[stgIOLoadPtr] = createVertexLoad(temp);
  }

  currentFunc->addInstrCacheToFront();
  return true;
}

bool PervertexInputVisitor::visit(SpirvFunction *sf, Phase phase) {
  // Add var mapping here. First function would be main.
  currentFunc = sf;
  inEntryFunctionWrapper = false;
  if (sf->isEntryFunctionWrapper()) {
    if (phase == Phase::Done) {
      reassignInputsLocation();
    } else {
      inEntryFunctionWrapper = true;
      return true;
    }
  }
  if (phase == Phase::Done) {
    currentFunc->addInstrCacheToFront();
  } else {
    // Refine variables and parameters. Add vtx0 for them.
    // (Those param and var haven't been expanded at this point).
    for (auto *var : currentFunc->getVariables()) {
      if (!var->isNoninterpolated())
        continue;
      auto *vtx0 =
          createProvokingVertexAccessChain(var, 0,
                                           var->getAstResultType());
      m_instrReplaceMap[var] = vtx0;
    }
    for (auto *param : currentFunc->getParameters()) {
      if (!param->isNoninterpolated())
        continue;
      auto *vtx0 =
          createProvokingVertexAccessChain(param, 0,
                                           param->getAstResultType());
      auto paramName = param->getDebugName().str() + "_perVertexParam0";
      m_instrReplaceMap[param] =
          createFirstPerVertexVar(vtx0, paramName);
    }
  }
  return true;
}

/// Spirv Instruction check and ptr replacement if needed.
bool PervertexInputVisitor::visit(SpirvVariable *inst) {
  if (expandNointerpVarAndParam(inst) &&
      inst->getStorageClass() == spv::StorageClass::Input)
    spirvBuilder.decoratePerVertexKHR(inst, inst->getSourceLocation());
  return true;
}

bool PervertexInputVisitor::visit(SpirvFunctionParameter *inst) {
  expandNointerpVarAndParam(inst);
  return true;
}

bool PervertexInputVisitor::visit(SpirvCompositeExtract *inst) {
  if (inst->isNoninterpolated() &&
      !isNotExpandedVectorAccess(inst->getComposite()->getAstResultType(),
                                 inst->getAstResultType())) {
    int idx = appendIndexZeroAt(inst->getComposite()->getAstResultType(),
                      inst->getIndexes());
    inst->insertIndex(0, idx);
    inst->setNoninterpolated(false);
  }
  return true;
}

bool PervertexInputVisitor::visit(SpirvAccessChain *inst) {
  llvm::SmallVector<uint32_t, 4> indexes;
  SpirvInstruction *zero =
      spirvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto idx = dyn_cast<SpirvAccessChain>(inst)->getIndexes();
  for (auto i : idx){
    if (isa<SpirvConstantInteger>(i)){
      indexes.push_back(dyn_cast<SpirvConstantInteger>(i)->getValue().getZExtValue());
    }
  }

  if (inst->isNoninterpolated() &&
      !isNotExpandedVectorAccess(inst->getBase()->getAstResultType(),
                                 inst->getAstResultType())) {
    // Not add extra index to a vector channel access.
    int idx = appendIndexZeroAt(inst->getBase()->getAstResultType(), indexes);
    inst->insertIndex(zero, idx);
    inst->setNoninterpolated(false);
  }
  return true;
}

// Only replace arg if not in entry function.
// If an expanded variable/parameter is passed to a function,
// recreate a pair of S/L instructions.
bool PervertexInputVisitor::visit(SpirvFunctionCall *inst) {
  /// Replace each use of pervertex inputs with its vertex0 element within functions.
  /// But pass it as an array if meet function call.
  if (inEntryFunctionWrapper)
    return true;
  /// Load/Store instructions related to this arg may have been replaced with other
  /// instructions, so we need to get its original mapped variables.
  for (auto *arg : inst->getArgs())
    if (currentFunc->getMappedFuncParam(arg))
    {
      createVertexStore(arg, createVertexLoad(currentFunc->getMappedFuncParam(arg)));
    }
  currentFunc->addInstrCacheToFront();
  return true;
}

} // end namespace spirv
} // end namespace clang
