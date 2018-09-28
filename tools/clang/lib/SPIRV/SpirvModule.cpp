//===--- SpirvModule.cpp - SPIR-V Module Implementation ----------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvModule.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

SpirvModule::SpirvModule()
    : bound(1), memoryModel(nullptr), debugSource(nullptr) {}

bool SpirvModule::visit(Visitor *visitor) {
  if (!visitor->visit(this, Visitor::Phase::Init))
    return false;

  for (auto *cap : capabilities)
    if (!cap->invokeVisitor(visitor))
      return false;

  for (auto ext : extensions)
    if (!ext->invokeVisitor(visitor))
      return false;

  for (auto extInstSet : extInstSets)
    if (!extInstSet->invokeVisitor(visitor))
      return false;

  if (!memoryModel->invokeVisitor(visitor))
    return false;

  for (auto entryPoint : entryPoints)
    if (!entryPoint->invokeVisitor(visitor))
      return false;

  for (auto execMode : executionModes)
    if (!execMode->invokeVisitor(visitor))
      return false;

  if (!debugSource->invokeVisitor(visitor))
    return false;
  for (auto debugName : debugNames)
    if (!debugName->invokeVisitor(visitor))
      return false;

  for (auto decoration : decorations)
    if (!decoration->invokeVisitor(visitor))
      return false;

  // TODO: type and constants

  for (auto var : variables)
    if (!var->invokeVisitor(visitor))
      return false;

  for (auto fn : functions)
    if (!fn->invokeVisitor(visitor))
      return false;

  if (!visitor->visit(this, Visitor::Phase::Done))
    return false;

  return true;
}

} // end namespace spirv
} // end namespace clang
