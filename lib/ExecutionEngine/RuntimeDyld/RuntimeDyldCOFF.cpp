//===-- RuntimeDyldCOFF.cpp - Run-time dynamic linker for MC-JIT -*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RuntimeDyldCOFF.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Implementation of COFF support for the MC-JIT runtime dynamic linker.     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "RuntimeDyldCOFF.h"
#include "Targets/RuntimeDyldCOFFX86_64.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Object/ObjectFile.h"

using namespace llvm;
using namespace llvm::object;

#define DEBUG_TYPE "dyld"

namespace {

class LoadedCOFFObjectInfo
    : public RuntimeDyld::LoadedObjectInfoHelper<LoadedCOFFObjectInfo> {
public:
  LoadedCOFFObjectInfo(RuntimeDyldImpl &RTDyld, unsigned BeginIdx,
                       unsigned EndIdx)
      : LoadedObjectInfoHelper(RTDyld, BeginIdx, EndIdx) {}

  OwningBinary<ObjectFile>
  getObjectForDebug(const ObjectFile &Obj) const override {
    return OwningBinary<ObjectFile>();
  }
};
}

namespace llvm {

std::unique_ptr<RuntimeDyldCOFF>
llvm::RuntimeDyldCOFF::create(Triple::ArchType Arch,
                              RuntimeDyld::MemoryManager &MemMgr,
                              RuntimeDyld::SymbolResolver &Resolver) {
  switch (Arch) {
  default:
    llvm_unreachable("Unsupported target for RuntimeDyldCOFF.");
    break;
  case Triple::x86_64:
    return make_unique<RuntimeDyldCOFFX86_64>(MemMgr, Resolver);
  }
}

std::unique_ptr<RuntimeDyld::LoadedObjectInfo>
RuntimeDyldCOFF::loadObject(const object::ObjectFile &O) {
  unsigned SectionStartIdx, SectionEndIdx;
  std::tie(SectionStartIdx, SectionEndIdx) = loadObjectImpl(O);
  return llvm::make_unique<LoadedCOFFObjectInfo>(*this, SectionStartIdx,
                                                 SectionEndIdx);
}

uint64_t RuntimeDyldCOFF::getSymbolOffset(const SymbolRef &Sym) {
  // The value in a relocatable COFF object is the offset.
  return Sym.getValue();
}

bool RuntimeDyldCOFF::isCompatibleFile(const object::ObjectFile &Obj) const {
  return Obj.isCOFF();
}

} // namespace llvm
