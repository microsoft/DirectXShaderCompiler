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

///< For expanded variables, we need to decide where to add an extra index zero
///< for SpirvAccessChain and SpirvCompositeExtract. This comes to three access
///< cases : 1. array element. 2. structure member 3. vector channel.
int PervertexInputVisitor::appendIndexZeroAt(
    QualType baseType, llvm::ArrayRef<uint32_t> instIndexes) {
  if (instIndexes.size() == 0)
    return 0;
  ///< 1. Array element
  if (baseType.getTypePtr()->isArrayType()) {
    int delta =
        appendIndexZeroAt(baseType->getAsArrayTypeUnsafe()->getElementType(),
                          instIndexes.slice(1));
    if (delta == 0)
      // swizzling at an array element in lowest level.
      return 0;
    else
      // intermediate array/struct element access.
      return 1 + delta;
  }
  ///< 2. structure member
  if (baseType.getTypePtr()->isStructureType()) {
    uint32_t idx = instIndexes[0];
    for (auto *f : baseType->getAs<RecordType>()->getDecl()->fields()) {
      if (idx == 0) {
        return 1 + appendIndexZeroAt(f->getType(), instIndexes.slice(1));
        break;
      }
      idx--;
    }
  }
  ///< 3. vector channel.
  return 0;
}

///< Expand nointerpolation decorated variables/parameters.
///< If a variable/parameter is passed from a decorated inputs, it should be
///< treated as nointerpolated too.
bool PervertexInputVisitor::expandNointerpVarAndParam(
    SpirvInstruction *spvInst) {
  QualType type = spvInst->getAstResultType();
  bool isExpanded = false;
  auto typePtr = type.getTypePtr();
  if (typePtr->isStructureType()) {
    /// Structure
    isExpanded = expandNointerpStructure(type, spvInst->isNoninterpolated());
  } else if (spvInst->isNoninterpolated()) {
    /// Normal type
    isExpanded = true;
    type = astContext.getConstantArrayType(type, llvm::APInt(32, 3),
                                           clang::ArrayType::Normal, 0);
    spvInst->setAstResultType(type);
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

SpirvInstruction *PervertexInputVisitor::createProvokingVertexAccessChain(
    SpirvInstruction *base, uint32_t index, QualType resultType) {
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
///< input.vtx0 within current function, but would be treated as an array will
///< pass to a function call.
SpirvInstruction *
PervertexInputVisitor::createVertexLoad(SpirvInstruction *base) {
  SpirvInstruction *loadPtr = new (context)
      SpirvLoad(base->getAstResultType(), base->getSourceLocation(), base,
                base->getSourceRange());
  loadPtr->setStorageClass(spv::StorageClass::Function);
  loadPtr->setLayoutRule(base->getLayoutRule());
  loadPtr->setRValue(true);
  currentFunc->addToInstructionCache(loadPtr);
  return loadPtr;
}

void PervertexInputVisitor::createVertexStore(SpirvInstruction *pt,
                                              SpirvInstruction *obj) {
  auto *storeInstr = new (context) SpirvStore(pt->getSourceLocation(), pt, obj);
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

bool PervertexInputVisitor::visit(SpirvModule *m, Phase phase) {
  if (!m->isPerVertexInterpMode())
    return false;
  currentMod = m;
  return true;
}

bool PervertexInputVisitor::visit(SpirvEntryPoint *ep) {
  // Add variable mapping here. First function would be main.
  currentFunc = ep->getEntryPoint();
  /// Refine stage in/out variables
  for (auto *var : currentMod->getVariables()) {
    if (!var->isNoninterpolated() ||
        var->getStorageClass() != spv::StorageClass::Input)
      continue;
    auto *stgIOLoadPtr = spirvBuilder.getPerVertexStgInput(var);
    if (!isa<SpirvBinaryOp>(stgIOLoadPtr)) {
      stgIOLoadPtr->setAstResultType(var->getAstResultType());
    }
  }
  return true;
}

bool PervertexInputVisitor::visit(SpirvFunction *sf, Phase phase) {
  // Add variable mapping here. First function would be main.
  currentFunc = sf;
  inEntryFunctionWrapper = false;
  if (sf->isEntryFunctionWrapper()) {
    if (phase != Phase::Done) {
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
          createProvokingVertexAccessChain(var, 0, var->getAstResultType());
      m_instrReplaceMap[var] = vtx0;
    }
    for (auto *param : currentFunc->getParameters()) {
      if (!param->isNoninterpolated())
        continue;
      auto *vtx0 =
          createProvokingVertexAccessChain(param, 0, param->getAstResultType());
      auto paramName = param->getDebugName().str() + "_perVertexParam0";
      m_instrReplaceMap[param] = createFirstPerVertexVar(vtx0, paramName);
    }
  }
  return true;
}

/// Spirv Instruction check and pointer replacement if needed.
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
  for (auto i : idx) {
    if (isa<SpirvConstantInteger>(i)) {
      indexes.push_back(
          dyn_cast<SpirvConstantInteger>(i)->getValue().getZExtValue());
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

// Only replace argument if not in entry function.
// If an expanded variable/parameter is passed to a function,
// recreate a pair of Store/Load instructions.
bool PervertexInputVisitor::visit(SpirvFunctionCall *inst) {
  /// Replace each use of pervertex inputs with its vertex0 element within
  /// functions. But pass it as an array if meet function call.
  if (inEntryFunctionWrapper)
    return true;
  /// Load/Store instructions related to this argument may have been replaced
  /// with other instructions, so we need to get its original mapped variables.
  for (auto *arg : inst->getArgs())
    if (currentFunc->getMappedFuncParam(arg)) {
      createVertexStore(arg,
                        createVertexLoad(currentFunc->getMappedFuncParam(arg)));
    }
  currentFunc->addInstrCacheToFront();
  return true;
}

} // end namespace spirv
} // end namespace clang
