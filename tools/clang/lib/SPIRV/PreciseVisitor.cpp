//===--- PreciseVisitor.cpp ------- Precise Visitor --------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PreciseVisitor.h"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvFunction.h"

namespace clang {
namespace spirv {

bool PreciseVisitor::visit(SpirvFunction *fn, Phase phase) {
  // Before going through the function instructions
  if (phase == Visitor::Phase::Init) {
    curFnRetValPrecise = fn->isPrecise();
  }
  return true;
}

bool PreciseVisitor::visit(SpirvReturn *inst) {
  if (inst->hasReturnValue()) {
    inst->getReturnValue()->setPrecise(curFnRetValPrecise);
  }
  return true;
}

bool PreciseVisitor::visit(SpirvVariable *var) {
  if (var->hasInitializer())
    var->getInitializer()->setPrecise(var->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvSelect *inst) {
  inst->getTrueObject()->setPrecise(inst->isPrecise());
  inst->getFalseObject()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvVectorShuffle *inst) {
  // If the result of a vector shuffle is 'precise', the vectors from which the
  // elements are chosen should also be 'precise'.
  if (inst->isPrecise()) {
    auto *vec1 = inst->getVec1();
    auto *vec2 = inst->getVec2();
    const auto vec1Type = vec1->getAstResultType();
    const auto vec2Type = vec2->getAstResultType();
    uint32_t vec1Size;
    uint32_t vec2Size;
    (void)isVectorType(vec1Type, nullptr, &vec1Size);
    (void)isVectorType(vec2Type, nullptr, &vec2Size);
    bool vec1ElemUsed = false;
    bool vec2ElemUsed = false;
    for (auto component : inst->getComponents()) {
      if (component < vec1Size)
        vec1ElemUsed = true;
      else
        vec2ElemUsed = true;
    }

    if (vec1ElemUsed)
      vec1->setPrecise();
    if (vec2ElemUsed)
      vec2->setPrecise();
  }
  return true;
}

bool PreciseVisitor::visit(SpirvBitFieldExtract *inst) {
  inst->getBase()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvBitFieldInsert *inst) {
  inst->getBase()->setPrecise(inst->isPrecise());
  inst->getInsert()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvAtomic *inst) {
  if (inst->isPrecise() && inst->hasValue())
    inst->getValue()->setPrecise();
  return true;
}

bool PreciseVisitor::visit(SpirvCompositeConstruct *inst) {
  if (inst->isPrecise())
    for (auto *consituent : inst->getConstituents())
      consituent->setPrecise();
  return true;
}

bool PreciseVisitor::visit(SpirvCompositeExtract *inst) {
  inst->getComposite()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvCompositeInsert *inst) {
  inst->getComposite()->setPrecise(inst->isPrecise());
  inst->getObject()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvLoad *inst) {
  // If the instruction result is precise, the pointer we're loading from should
  // also be marked as precise.
  if (inst->isPrecise())
    inst->getPointer()->setPrecise();

  return true;
}

bool PreciseVisitor::visit(SpirvStore *inst) {
  // If the 'pointer' to which we are storing is marked as 'precise', the object
  // we are storing should also be marked as 'precise'.
  // Note that the 'pointer' may either be an 'OpVariable' or it might be the
  // result of one or more access chains (in which case we should figure out if
  // the 'base' of the access chain is 'precise').
  auto *ptr = inst->getPointer();
  auto *obj = inst->getObject();

  // The simple case (target is a precise variable).
  if (ptr->isPrecise()) {
    obj->setPrecise();
    return true;
  }

  while (auto *accessChain = llvm::dyn_cast<SpirvAccessChain>(ptr)) {
    if (accessChain->getBase()->isPrecise()) {
      obj->setPrecise();
      return true;
    }
    ptr = accessChain->getBase();
  }

  return true;
}

bool PreciseVisitor::visit(SpirvBinaryOp *inst) {
  bool isPrecise = inst->isPrecise();
  inst->getOperand1()->setPrecise(isPrecise);
  inst->getOperand2()->setPrecise(isPrecise);
  return true;
}

bool PreciseVisitor::visit(SpirvUnaryOp *inst) {
  inst->getOperand()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvNonUniformBinaryOp *inst) {
  inst->getArg1()->setPrecise(inst->isPrecise());
  inst->getArg2()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvNonUniformUnaryOp *inst) {
  inst->getArg()->setPrecise(inst->isPrecise());
  return true;
}

bool PreciseVisitor::visit(SpirvExtInst *inst) {
  if (inst->isPrecise())
    for (auto *operand : inst->getOperands())
      operand->setPrecise();
  return true;
}

} // end namespace spirv
} // end namespace clang
