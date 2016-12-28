//===- PassRegistry.cpp - Pass Registration Implementation ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PassRegistry.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the PassRegistry, with which passes are registered on//
// initialization, and supports the PassManager in dependency resolution.    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/PassRegistry.h"
#include "llvm/IR/Function.h"
#include "llvm/PassSupport.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/RWMutex.h"
#include <vector>

using namespace llvm;

// HLSL Change Starts - managed statics are tied to DLL lifetime
// Passes exist only in dxcompiler.dll or in a tool like opt that is updated.
//
// These usage patterns imply the following:
// - llvm_shutdown need not allow resurrection, as it's only called at the
//   end of main() or at DLL-unload time, and
// - because there is a fixed number of passes (dynamic loading is currently
//   unsupported), there is no need to make this thread-safe.
//
// A simple global initialized at DllMain-time will do (still does more work
// than we should likely perform though).
static uint32_t g_PassRegistryTid;
extern "C" uint32_t __stdcall GetCurrentThreadId(void);
static void CheckThreadId() {
  if (g_PassRegistryTid == 0)
    g_PassRegistryTid = GetCurrentThreadId();
  else
    assert(g_PassRegistryTid == GetCurrentThreadId() &&
      "else updating PassRegistry from incorrect thread");
}
// HLSL Change Ends

// FIXME: We use ManagedStatic to erase the pass registrar on shutdown.
// Unfortunately, passes are registered with static ctors, and having
// llvm_shutdown clear this map prevents successful resurrection after
// llvm_shutdown is run.  Ideally we should find a solution so that we don't
// leak the map, AND can still resurrect after shutdown.
static ManagedStatic<PassRegistry> PassRegistryObj;
PassRegistry *PassRegistry::getPassRegistry() {
  return &*PassRegistryObj;
}

//===----------------------------------------------------------------------===//
// Accessors
//

PassRegistry::~PassRegistry() {}

const PassInfo *PassRegistry::getPassInfo(const void *TI) const {
  // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change
  MapType::const_iterator I = PassInfoMap.find(TI);
  return I != PassInfoMap.end() ? I->second : nullptr;
}

const PassInfo *PassRegistry::getPassInfo(StringRef Arg) const {
  // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change
  StringMapType::const_iterator I = PassInfoStringMap.find(Arg);
  return I != PassInfoStringMap.end() ? I->second : nullptr;
}

//===----------------------------------------------------------------------===//
// Pass Registration mechanism
//

void PassRegistry::registerPass(const PassInfo &PI, bool ShouldFree) {
  CheckThreadId(); // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change
  bool Inserted =
      PassInfoMap.insert(std::make_pair(PI.getTypeInfo(), &PI)).second;
  assert(Inserted && "Pass registered multiple times!");
  (void)Inserted;
  PassInfoStringMap[PI.getPassArgument()] = &PI;

  // Notify any listeners.
  for (auto *Listener : Listeners)
    Listener->passRegistered(&PI);

  if (ShouldFree)
    ToFree.push_back(std::unique_ptr<const PassInfo>(&PI));
}

void PassRegistry::enumerateWith(PassRegistrationListener *L) {
  // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change
  for (auto PassInfoPair : PassInfoMap)
    L->passEnumerate(PassInfoPair.second);
}

/// Analysis Group Mechanisms.
void PassRegistry::registerAnalysisGroup(const void *InterfaceID,
                                         const void *PassID,
                                         PassInfo &Registeree, bool isDefault,
                                         bool ShouldFree) {
  PassInfo *InterfaceInfo = const_cast<PassInfo *>(getPassInfo(InterfaceID));
  if (!InterfaceInfo) {
    // First reference to Interface, register it now.
    registerPass(Registeree);
    InterfaceInfo = &Registeree;
  }
  assert(Registeree.isAnalysisGroup() &&
         "Trying to join an analysis group that is a normal pass!");

  if (PassID) {
    PassInfo *ImplementationInfo = const_cast<PassInfo *>(getPassInfo(PassID));
    assert(ImplementationInfo &&
           "Must register pass before adding to AnalysisGroup!");

    CheckThreadId(); // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change

    // Make sure we keep track of the fact that the implementation implements
    // the interface.
    ImplementationInfo->addInterfaceImplemented(InterfaceInfo);

    if (isDefault) {
      assert(InterfaceInfo->getNormalCtor() == nullptr &&
             "Default implementation for analysis group already specified!");
      assert(
          ImplementationInfo->getNormalCtor() &&
          "Cannot specify pass as default if it does not have a default ctor");
      InterfaceInfo->setNormalCtor(ImplementationInfo->getNormalCtor());
      InterfaceInfo->setTargetMachineCtor(
          ImplementationInfo->getTargetMachineCtor());
    }
  }

  if (ShouldFree)
    ToFree.push_back(std::unique_ptr<const PassInfo>(&Registeree));
}

void PassRegistry::addRegistrationListener(PassRegistrationListener *L) {
  CheckThreadId(); // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change
  Listeners.push_back(L);
}

void PassRegistry::removeRegistrationListener(PassRegistrationListener *L) {
  CheckThreadId(); // sys::SmartScopedReader<true> Guard(Lock); // HLSL Change

  auto I = std::find(Listeners.begin(), Listeners.end(), L);
  Listeners.erase(I);
}
