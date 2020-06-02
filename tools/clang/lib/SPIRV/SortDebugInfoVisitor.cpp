//===--- SortDebugInfoVisitor.cpp - Valid order debug instrs -----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SortDebugInfoVisitor.h"

namespace clang {
namespace spirv {

void SortDebugInfoVisitor::whileEachOperandOfDebugInstruction(
    SpirvDebugInstruction *di,
    llvm::function_ref<bool(SpirvDebugInstruction *)> visitor) {
  if (di == nullptr)
    return;
  if (di->getDebugType() != nullptr) {
    if (!visitor(di->getDebugType()))
      return;
  }
  if (di->getParentScope() != nullptr) {
    if (!visitor(di->getParentScope()))
      return;
  }

  switch (di->getKind()) {
  case SpirvInstruction::IK_DebugCompilationUnit: {
    SpirvDebugCompilationUnit *inst = dyn_cast<SpirvDebugCompilationUnit>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getDebugSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugFunctionDecl: {
    SpirvDebugFunctionDeclaration *inst =
        dyn_cast<SpirvDebugFunctionDeclaration>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugFunction: {
    SpirvDebugFunction *inst = dyn_cast<SpirvDebugFunction>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getSource()))
      break;
    if (!visitor(inst->getDebugInfoNone()))
      break;
  } break;
  case SpirvInstruction::IK_DebugLocalVariable: {
    SpirvDebugLocalVariable *inst = dyn_cast<SpirvDebugLocalVariable>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugGlobalVariable: {
    SpirvDebugGlobalVariable *inst = dyn_cast<SpirvDebugGlobalVariable>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugExpression: {
    SpirvDebugExpression *inst = dyn_cast<SpirvDebugExpression>(di);
    assert(inst != nullptr);
    for (auto *op : inst->getOperations())
      if (!visitor(op))
        break;
  } break;
  case SpirvInstruction::IK_DebugLexicalBlock: {
    SpirvDebugLexicalBlock *inst = dyn_cast<SpirvDebugLexicalBlock>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugTypeArray: {
    SpirvDebugTypeArray *inst = dyn_cast<SpirvDebugTypeArray>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getElementType()))
      break;
  } break;
  case SpirvInstruction::IK_DebugTypeVector: {
    SpirvDebugTypeVector *inst = dyn_cast<SpirvDebugTypeVector>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getElementType()))
      break;
  } break;
  case SpirvInstruction::IK_DebugTypeFunction: {
    SpirvDebugTypeFunction *inst = dyn_cast<SpirvDebugTypeFunction>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getReturnType()))
      break;
    for (auto *param : inst->getParamTypes())
      if (!visitor(param))
        break;
  } break;
  case SpirvInstruction::IK_DebugTypeComposite: {
    SpirvDebugTypeComposite *inst = dyn_cast<SpirvDebugTypeComposite>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getSource()))
      break;
    if (!visitor(inst->getDebugInfoNone()))
      break;
  } break;
  case SpirvInstruction::IK_DebugTypeMember: {
    SpirvDebugTypeMember *inst = dyn_cast<SpirvDebugTypeMember>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getType()))
      break;
    if (!visitor(inst->getSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugTypeTemplate: {
    SpirvDebugTypeTemplate *inst = dyn_cast<SpirvDebugTypeTemplate>(di);
    assert(inst != nullptr);
    bool success = true;
    for (auto *param : inst->getParams()) {
      if (!visitor(param)) {
        success = false;
        break;
      }
    }
    if (success && !visitor(inst->getTarget()))
      break;
  } break;
  case SpirvInstruction::IK_DebugTypeTemplateParameter: {
    SpirvDebugTypeTemplateParameter *inst =
        dyn_cast<SpirvDebugTypeTemplateParameter>(di);
    assert(inst != nullptr);
    if (!visitor(inst->getActualType()))
      break;
    if (!visitor(inst->getSource()))
      break;
  } break;
  case SpirvInstruction::IK_DebugInfoNone:
  case SpirvInstruction::IK_DebugSource:
  case SpirvInstruction::IK_DebugOperation:
  case SpirvInstruction::IK_DebugTypeBasic:
    break;
  default:
    // DebugDeclare and DebugScope must be placed within a function.
    assert(false && "unsupported debug instruction");
    break;
  }
}

bool SortDebugInfoVisitor::visit(SpirvModule *mod, Phase phase) {
  if (phase == Phase::Done)
    return true;

  auto &debugInstructions = mod->getDebugInfo();
  auto numberOfDebugInstrs = debugInstructions.size();
  (void)numberOfDebugInstrs;

  // Collect nodes without predecessor.
  llvm::SmallSet<SpirvDebugInstruction *, 32> visited;
  for (auto *di : debugInstructions) {
    whileEachOperandOfDebugInstruction(
        di, [&visited](SpirvDebugInstruction *operand) {
          if (operand != nullptr)
            visited.insert(operand);
          return true;
        });
  }
  llvm::SmallVector<SpirvDebugInstruction *, 32> stack;
  for (auto *di : debugInstructions) {
    if (visited.count(di) == 0)
      stack.push_back(di);
  }

  // Sort debug instructions in a post order. We puts successors in the first
  // places of `debugInstructions`. For example, `DebugInfoNone` does not have
  // any operand, which means it does not have any successors. We have to place
  // it earlier than the instructions using it.
  debugInstructions.clear();
  visited.clear();
  while (!stack.empty()) {
    auto *di = stack.back();
    visited.insert(di);
    whileEachOperandOfDebugInstruction(
        di, [&visited, &stack](SpirvDebugInstruction *operand) {
          if (operand != nullptr && visited.count(operand) == 0) {
            stack.push_back(operand);
            return false;
          }
          return true;
        });
    if (stack.back() == di) {
      debugInstructions.push_back(di);
      stack.pop_back();
    }
  }

  // The sort result must have the same number of debug instructions.
  assert(numberOfDebugInstrs == debugInstructions.size());

  return true;
}

} // end namespace spirv
} // end namespace clang
