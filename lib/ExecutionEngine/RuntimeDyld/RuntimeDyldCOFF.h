//===-- RuntimeDyldCOFF.h - Run-time dynamic linker for MC-JIT ---*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RuntimeDyldCOFF.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// COFF support for MC-JIT runtime dynamic linker.                           //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_RUNTIME_DYLD_COFF_H
#define LLVM_RUNTIME_DYLD_COFF_H

#include "RuntimeDyldImpl.h"
#include "llvm/ADT/DenseMap.h"

#define DEBUG_TYPE "dyld"

using namespace llvm;

namespace llvm {

// Common base class for COFF dynamic linker support.
// Concrete subclasses for each target can be found in ./Targets.
class RuntimeDyldCOFF : public RuntimeDyldImpl {

public:
  std::unique_ptr<RuntimeDyld::LoadedObjectInfo>
  loadObject(const object::ObjectFile &Obj) override;
  bool isCompatibleFile(const object::ObjectFile &Obj) const override;

  static std::unique_ptr<RuntimeDyldCOFF>
  create(Triple::ArchType Arch, RuntimeDyld::MemoryManager &MemMgr,
         RuntimeDyld::SymbolResolver &Resolver);

protected:
  RuntimeDyldCOFF(RuntimeDyld::MemoryManager &MemMgr,
                  RuntimeDyld::SymbolResolver &Resolver)
    : RuntimeDyldImpl(MemMgr, Resolver) {}
  uint64_t getSymbolOffset(const SymbolRef &Sym);
};

} // end namespace llvm

#undef DEBUG_TYPE

#endif
