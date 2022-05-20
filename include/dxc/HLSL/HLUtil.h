///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLUtil.h                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// HL helper functions.                                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "llvm/ADT/SetVector.h"

namespace llvm {
class Function;
class Value;
class MemCpyInst;
} // namespace llvm

namespace hlsl {
class DxilTypeSystem;

namespace hlutil {

struct PointerStatus {
  /// Keep track of what stores to the pointer look like.
  enum class StoredType {
    /// There is no store to this pointer.  It can thus be marked constant.
    NotStored,

    /// This ptr is a global, and is stored to, but the only thing stored is the
    /// constant it
    /// was initialized with. This is only tracked for scalar globals.
    InitializedOnly,

    /// This ptr is only assigned by a memcpy.
    MemcopyStoredOnce,

    /// This ptr is stored to by multiple values or something else that we
    /// cannot track.
    MultipleStores
  } storedType;
  /// Keep track of what loaded from the pointer look like.
  enum class LoadedType {
    /// There is no load of this pointer.  It can thus be marked constant.
    NotLoaded,

    /// This ptr is only used by a memcpy.
    MemcopyLoadedOnce,

    /// This ptr is loaded by multiple instructions or something else that we
    /// cannot track.
    MultipleLoads
  } loadedType;
  /// Memcpy which this ptr is used.
  llvm::SetVector<llvm::MemCpyInst *> memcpySet;
  /// Memcpy which uses this ptr as dest.
  llvm::MemCpyInst *StoringMemcpy;
  /// Memcpy which uses this ptr as src.
  llvm::MemCpyInst *LoadingMemcpy;
  /// These start out null/false.  When the first accessing function is noticed,
  /// it is recorded. When a second different accessing function is noticed,
  /// HasMultipleAccessingFunctions is set to true.
  const llvm::Function *AccessingFunction;
  bool HasMultipleAccessingFunctions;
  /// Size of the ptr.
  unsigned Size;
  llvm::Value *Ptr;
  // Just check load store.
  bool bLoadStoreOnly;

  void analyze(DxilTypeSystem &typeSys, bool bStructElt);

  PointerStatus(llvm::Value *ptr, unsigned size, bool bLdStOnly);
  void MarkAsMultiStored();
  void MarkAsMultiLoaded();
  bool HasStored();
  bool HasLoaded();
};

} // namespace hlutil

} // namespace hlsl
