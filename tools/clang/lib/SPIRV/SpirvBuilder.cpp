//===--- SpirvBuilder.cpp - SPIR-V Builder Implementation --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBuilder.h"
#include "llvm/Support/MathExtras.h"

namespace clang {
namespace spirv {

SpirvBuilder::SpirvBuilder(SpirvContext &ctx)
    : context(ctx), module(nullptr), function(nullptr) {
  module = new (context) SpirvModule;
}

SpirvFunction *SpirvBuilder::beginFunction(QualType returnType,
                                           SourceLocation loc,
                                           llvm::StringRef funcName) {
  assert(!function && "found nested function");
  function = new (context) SpirvFunction(
      returnType, /*id*/ 0, spv::FunctionControlMask::MaskNone, loc, funcName);
  return function;
}

SpirvFunctionParameter *SpirvBuilder::addFnParam(QualType ptrType,
                                                 SourceLocation loc,
                                                 llvm::StringRef name) {
  assert(function && "found detached parameter");
  auto *param = new (context) SpirvFunctionParameter(ptrType, /*id*/ 0, loc);
  param->setDebugName(name);
  function->addParameter(param);
  return param;
}

SpirvVariable *SpirvBuilder::addFnVar(QualType valueType, SourceLocation loc,
                                      llvm::StringRef name,
                                      SpirvInstruction *init) {
  assert(function && "found detached local variable");
  auto *var = new (context) SpirvVariable(valueType, /*id*/ 0, loc,
                                          spv::StorageClass::Function, init);
  var->setDebugName(name);
  function->addVariable(var);
  return var;
}

void SpirvBuilder::endFunction() {
  assert(function && "no active function");

  // Move all basic blocks into the current function.
  // TODO: we should adjust the order the basic blocks according to
  // SPIR-V validation rules.
  for (auto *bb : basicBlocks) {
    function->addBasicBlock(bb);
  }
  basicBlocks.clear();

  module->addFunction(function);
  function = nullptr;
  insertPoint = nullptr;
}

SpirvBasicBlock *SpirvBuilder::createBasicBlock(llvm::StringRef name) {
  assert(function && "found detached basic block");
  auto *bb = new (context) SpirvBasicBlock(/*id*/ 0, name);
  basicBlocks.push_back(bb);
  return bb;
}

void SpirvBuilder::addSuccessor(SpirvBasicBlock *successorBB) {
  assert(insertPoint && "null insert point");
  insertPoint->addSuccessor(successorBB);
}

void SpirvBuilder::setMergeTarget(SpirvBasicBlock *mergeLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->setMergeTarget(mergeLabel);
}

void SpirvBuilder::setContinueTarget(SpirvBasicBlock *continueLabel) {
  assert(insertPoint && "null insert point");
  insertPoint->setContinueTarget(continueLabel);
}

} // end namespace spirv
} // end namespace clang
