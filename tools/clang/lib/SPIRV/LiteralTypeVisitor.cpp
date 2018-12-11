//===--- LiteralTypeVisitor.cpp - Literal Type Visitor -----------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "LiteralTypeVisitor.h"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvBuilder.h"

namespace clang {
namespace spirv {

bool LiteralTypeVisitor::visit(SpirvFunction *fn, Phase phase) {
  assert(fn);

  // Before going through the function instructions
  if (phase == Visitor::Phase::Init) {
    curFnAstReturnType = fn->getAstReturnType();
  }

  return true;
}

bool LiteralTypeVisitor::isLiteralLargerThan32Bits(
    SpirvConstantInteger *constant) {
  assert(constant->hasAstResultType());
  QualType type = constant->getAstResultType();
  const bool isSigned = type->isSignedIntegerType();
  const llvm::APInt &value = constant->getValue();
  return (isSigned && !value.isSignedIntN(32)) ||
         (!isSigned && !value.isIntN(32));
}

bool LiteralTypeVisitor::canDeduceTypeFromLitType(QualType litType,
                                                  QualType newType) {
  if (litType == QualType() || newType == QualType() || litType == newType)
    return false;
  if (!isLitTypeOrVecOfLitType(litType))
    return false;
  if (isLitTypeOrVecOfLitType(newType))
    return false;

  if (litType->isFloatingType() && newType->isFloatingType())
    return true;
  if ((litType->isIntegerType() && !litType->isBooleanType()) &&
      (newType->isIntegerType() && !newType->isBooleanType()))
    return true;

  {
    QualType elemType1 = {};
    uint32_t elemCount1 = 0;
    QualType elemType2 = {};
    uint32_t elemCount2 = 0;
    if (isVectorType(litType, &elemType1, &elemCount1) &&
        isVectorType(newType, &elemType2, &elemCount2))
      return elemCount1 == elemCount2 &&
             canDeduceTypeFromLitType(elemType1, elemType2);
  }

  return false;
}

void LiteralTypeVisitor::updateTypeForInstruction(SpirvInstruction *inst,
                                                  QualType newType) {
  if (!inst)
    return;

  // We may only update LitInt to Int type and LitFloat to Float type.
  if (!canDeduceTypeFromLitType(inst->getAstResultType(), newType))
    return;

  // Since LiteralTypeVisitor is run before lowering the types, we can simply
  // update the AST result-type of the instruction to the new type. In the case
  // of the instruction being a constant instruction, since we do not have
  // unique constants at this point, chaing the QualType of the constant
  // instruction is safe.
  inst->setAstResultType(newType);
}

bool LiteralTypeVisitor::visitInstruction(SpirvInstruction *instr) {
  // Instructions that don't have custom visitors cannot help with deducing the
  // real type from the literal type.
  return true;
}

bool LiteralTypeVisitor::visit(SpirvVariable *var) {
  updateTypeForInstruction(var->getInitializer(), var->getAstResultType());
  return true;
}

bool LiteralTypeVisitor::visit(SpirvAtomic *inst) {
  const auto resultType = inst->getAstResultType();
  updateTypeForInstruction(inst->getValue(), resultType);
  updateTypeForInstruction(inst->getComparator(), resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvUnaryOp *inst) {
  // Do not try to make conclusions about types for bitwidth conversion
  // operations.
  // TODO: We can do more to deduce information in OpBitCast.
  const auto opcode = inst->getopcode();
  if (opcode == spv::Op::OpUConvert || opcode == spv::Op::OpSConvert ||
      opcode == spv::Op::OpFConvert || opcode == spv::Op::OpBitcast) {
    return true;
  }

  const auto resultType = inst->getAstResultType();
  auto *arg = inst->getOperand();
  const auto argType = arg->getAstResultType();

  // OpNot, OpSNegate, and OpConvertXToY operations change the type, but may not
  // change the bitwidth. So, for these operations, we can use the result type's
  // bitwidth as a hint for the operand's bitwidth.
  // --> get signedness and vector size (if any) from operand
  // --> get bitwidth from result type
  if (opcode == spv::Op::OpConvertFToU || opcode == spv::Op::OpConvertFToS ||
      opcode == spv::Op::OpConvertSToF || opcode == spv::Op::OpConvertUToF ||
      opcode == spv::Op::OpNot || opcode == spv::Op::OpSNegate) {
    if (isLitTypeOrVecOfLitType(argType) &&
        !isLitTypeOrVecOfLitType(resultType)) {
      const uint32_t resultTypeBitwidth = getElementSpirvBitwidth(
          astContext, resultType, spvOptions.enable16BitTypes);
      const QualType newType =
          getTypeWithCustomBitwidth(astContext, argType, resultTypeBitwidth);
      updateTypeForInstruction(arg, newType);
      return true;
    }
  }

  // In all other cases, try to use the result type as a hint.
  updateTypeForInstruction(arg, resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvBinaryOp *inst) {
  const auto resultType = inst->getAstResultType();
  const auto op = inst->getopcode();
  auto *operand1 = inst->getOperand1();
  auto *operand2 = inst->getOperand2();

  // We should not modify operand2 type in these operations:
  if (op == spv::Op::OpShiftRightLogical ||
      op == spv::Op::OpShiftRightArithmetic ||
      op == spv::Op::OpShiftLeftLogical) {
    // Base (arg1) should have the same type as result type
    updateTypeForInstruction(inst->getOperand1(), resultType);
    // The shitf amount (arg2) cannot be a 64-bit type for a 32-bit base!
    updateTypeForInstruction(inst->getOperand2(), resultType);
    return true;
  }

  // The following operations have a boolean return type, so we cannot deduce
  // anything about the operand type from the result type. However, the two
  // operands in these operations must have the same bitwidth.
  if (op == spv::Op::OpIEqual || op == spv::Op::OpINotEqual ||
      op == spv::Op::OpUGreaterThan || op == spv::Op::OpSGreaterThan ||
      op == spv::Op::OpUGreaterThanEqual ||
      op == spv::Op::OpSGreaterThanEqual || op == spv::Op::OpULessThan ||
      op == spv::Op::OpSLessThan || op == spv::Op::OpULessThanEqual ||
      op == spv::Op::OpSLessThanEqual || op == spv::Op::OpFOrdEqual ||
      op == spv::Op::OpFUnordEqual || op == spv::Op::OpFOrdNotEqual ||
      op == spv::Op::OpFUnordNotEqual || op == spv::Op::OpFOrdLessThan ||
      op == spv::Op::OpFUnordLessThan || op == spv::Op::OpFOrdGreaterThan ||
      op == spv::Op::OpFUnordGreaterThan ||
      op == spv::Op::OpFOrdLessThanEqual ||
      op == spv::Op::OpFUnordLessThanEqual ||
      op == spv::Op::OpFOrdGreaterThanEqual ||
      op == spv::Op::OpFUnordGreaterThanEqual) {
    if (operand1->hasAstResultType() && operand2->hasAstResultType()) {
      const auto operand1Type = operand1->getAstResultType();
      const auto operand2Type = operand2->getAstResultType();
      bool isLitOp1 = isLitTypeOrVecOfLitType(operand1Type);
      bool isLitOp2 = isLitTypeOrVecOfLitType(operand2Type);

      if (isLitOp1 && !isLitOp2) {
        const uint32_t operand2Bitwidth = getElementSpirvBitwidth(
            astContext, operand2Type, spvOptions.enable16BitTypes);
        const QualType newType = getTypeWithCustomBitwidth(
            astContext, operand1Type, operand2Bitwidth);
        updateTypeForInstruction(operand1, newType);
        return true;
      }
      if (isLitOp2 && !isLitOp1) {
        const uint32_t operand1Bitwidth = getElementSpirvBitwidth(
            astContext, operand1Type, spvOptions.enable16BitTypes);
        const QualType newType = getTypeWithCustomBitwidth(
            astContext, operand2Type, operand1Bitwidth);
        updateTypeForInstruction(operand2, newType);
        return true;
      }
    }
  }

  updateTypeForInstruction(operand1, resultType);
  updateTypeForInstruction(operand2, resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvBitFieldInsert *inst) {
  const auto resultType = inst->getAstResultType();
  updateTypeForInstruction(inst->getBase(), resultType);
  updateTypeForInstruction(inst->getInsert(), resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvBitFieldExtract *inst) {
  const auto resultType = inst->getAstResultType();
  updateTypeForInstruction(inst->getBase(), resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvSelect *inst) {
  const auto resultType = inst->getAstResultType();
  updateTypeForInstruction(inst->getTrueObject(), resultType);
  updateTypeForInstruction(inst->getFalseObject(), resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvVectorShuffle *inst) {
  const auto resultType = inst->getAstResultType();
  if (inst->hasAstResultType() && !isLitTypeOrVecOfLitType(resultType)) {
    auto *vec1 = inst->getVec1();
    auto *vec2 = inst->getVec1();
    assert(vec1 && vec2);
    QualType resultElemType = {};
    uint32_t resultElemCount = 0;
    QualType vec1ElemType = {};
    uint32_t vec1ElemCount = 0;
    QualType vec2ElemType = {};
    uint32_t vec2ElemCount = 0;
    (void)isVectorType(resultType, &resultElemType, &resultElemCount);
    (void)isVectorType(vec1->getAstResultType(), &vec1ElemType, &vec1ElemCount);
    (void)isVectorType(vec2->getAstResultType(), &vec2ElemType, &vec2ElemCount);
    if (isLitTypeOrVecOfLitType(vec1ElemType)) {
      updateTypeForInstruction(
          vec1, astContext.getExtVectorType(resultElemType, vec1ElemCount));
    }
    if (isLitTypeOrVecOfLitType(vec2ElemType)) {
      updateTypeForInstruction(
          vec2, astContext.getExtVectorType(resultElemType, vec2ElemCount));
    }
  }
  return true;
}

bool LiteralTypeVisitor::visit(SpirvNonUniformUnaryOp *inst) {
  // Went through each non-uniform binary operation and made sure the following
  // does not result in a wrong type deduction.
  updateTypeForInstruction(inst->getArg(), inst->getAstResultType());
  return true;
}

bool LiteralTypeVisitor::visit(SpirvNonUniformBinaryOp *inst) {
  // Went through each non-uniform unary operation and made sure the following
  // does not result in a wrong type deduction.
  updateTypeForInstruction(inst->getArg1(), inst->getAstResultType());
  return true;
}

bool LiteralTypeVisitor::visit(SpirvStore *inst) {
  auto *object = inst->getObject();
  auto *pointer = inst->getPointer();
  if (pointer->hasAstResultType()) {
    QualType type = pointer->getAstResultType();
    if (const auto *ptrType = type->getAs<PointerType>())
      type = ptrType->getPointeeType();
    updateTypeForInstruction(object, type);
  } else if (pointer->hasResultType()) {
    if (auto *ptrType = dyn_cast<HybridPointerType>(pointer->getResultType())) {
      QualType type = ptrType->getPointeeType();
      updateTypeForInstruction(object, type);
    }
  }
  return true;
}

bool LiteralTypeVisitor::visit(SpirvConstantComposite *inst) {
  const auto resultType = inst->getAstResultType();
  llvm::SmallVector<SpirvInstruction *, 4> constituents(
      inst->getConstituents().begin(), inst->getConstituents().end());
  updateTypeForCompositeMembers(resultType, constituents);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvComposite *inst) {
  const auto resultType = inst->getAstResultType();
  updateTypeForCompositeMembers(resultType, inst->getConstituents());
  return true;
}

bool LiteralTypeVisitor::visit(SpirvCompositeExtract *inst) {
  const auto resultType = inst->getAstResultType();
  auto *base = inst->getComposite();
  const auto baseType = base->getAstResultType();
  if (isLitTypeOrVecOfLitType(baseType) &&
      !isLitTypeOrVecOfLitType(resultType)) {
    const uint32_t resultTypeBitwidth = getElementSpirvBitwidth(
        astContext, resultType, spvOptions.enable16BitTypes);
    const QualType newType =
        getTypeWithCustomBitwidth(astContext, baseType, resultTypeBitwidth);
    updateTypeForInstruction(base, newType);
  }

  return true;
}

bool LiteralTypeVisitor::updateTypeForCompositeMembers(
    QualType compositeType, llvm::ArrayRef<SpirvInstruction *> constituents) {

  if (compositeType == QualType())
    return true;

  // The constituents are the top level objects that create the result type.
  // The result type may be one of the following:
  // Vector, Array, Matrix, Struct

  // TODO: This method is currently not recursive. We can use recursion if
  // absolutely necessary.

  { // Vector case
    QualType elemType = {};
    if (isVectorType(compositeType, &elemType)) {
      for (auto *constituent : constituents)
        updateTypeForInstruction(constituent, elemType);
      return true;
    }
  }

  { // Array case
    if (const auto *arrType = dyn_cast<ConstantArrayType>(compositeType)) {
      for (auto *constituent : constituents)
        updateTypeForInstruction(constituent, arrType->getElementType());
      return true;
    }
  }

  { // Matrix case
    QualType elemType = {};
    if (isMxNMatrix(compositeType, &elemType)) {
      for (auto *constituent : constituents) {
        // Each constituent is a matrix column (a vector)
        uint32_t colSize = 0;
        if (isVectorType(constituent->getAstResultType(), nullptr, &colSize)) {
          QualType newType = astContext.getExtVectorType(elemType, colSize);
          updateTypeForInstruction(constituent, newType);
        }
      }
      return true;
    }
  }

  { // Struct case
    if (const auto *structType = compositeType->getAs<RecordType>()) {
      const auto *decl = structType->getDecl();
      size_t i = 0;
      for (const auto *field : decl->fields()) {
        updateTypeForInstruction(constituents[i], field->getType());
        ++i;
      }
      return true;
    }
  }

  return true;
}

bool LiteralTypeVisitor::visit(SpirvAccessChain *inst) {
  for (auto *index : inst->getIndexes()) {
    if (auto *constInt = dyn_cast<SpirvConstantInteger>(index)) {
      if (!isLiteralLargerThan32Bits(constInt)) {
        updateTypeForInstruction(
            constInt, constInt->getAstResultType()->isSignedIntegerType()
                          ? astContext.IntTy
                          : astContext.UnsignedIntTy);
      }
    }
  }
  return true;
}

bool LiteralTypeVisitor::visit(SpirvExtInst *inst) {
  // Result type of the instruction can provide a hint about its operands. e.g.
  // OpExtInst %float %glsl_set Pow %double_2 %double_12
  // should be evaluated as:
  // OpExtInst %float %glsl_set Pow %float_2 %float_12
  const auto resultType = inst->getAstResultType();
  for (auto *operand : inst->getOperands())
    updateTypeForInstruction(operand, resultType);
  return true;
}

bool LiteralTypeVisitor::visit(SpirvReturn *inst) {
  if (inst->hasReturnValue()) {
    updateTypeForInstruction(inst->getReturnValue(), curFnAstReturnType);
  }
  return true;
}

bool LiteralTypeVisitor::visit(SpirvCompositeInsert *inst) {
  const auto resultType = inst->getAstResultType();
  updateTypeForInstruction(inst->getComposite(), resultType);
  updateTypeForInstruction(inst->getObject(),
                           getElementType(astContext, resultType));
  return true;
}

bool LiteralTypeVisitor::visit(SpirvImageOp *inst) {
  if (inst->isImageWrite() && inst->hasAstResultType()) {
    const auto sampledType =
        hlsl::GetHLSLResourceResultType(inst->getAstResultType());
    updateTypeForInstruction(inst->getTexelToWrite(), sampledType);
  }
  return true;
}

} // end namespace spirv
} // namespace clang
