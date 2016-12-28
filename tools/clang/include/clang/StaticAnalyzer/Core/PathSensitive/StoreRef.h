//== StoreRef.h - Smart pointer for store objects ---------------*- C++ -*--==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// StoreRef.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defined the type StoreRef.                                     //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_STOREREF_H
#define LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_STOREREF_H

#include <cassert>

namespace clang {
namespace ento {
  
/// Store - This opaque type encapsulates an immutable mapping from
///  locations to values.  At a high-level, it represents the symbolic
///  memory model.  Different subclasses of StoreManager may choose
///  different types to represent the locations and values.
typedef const void *Store;
  
class StoreManager;
  
class StoreRef {
  Store store;
  StoreManager &mgr;
public:
  StoreRef(Store, StoreManager &);
  StoreRef(const StoreRef &);
  StoreRef &operator=(StoreRef const &);
  
  bool operator==(const StoreRef &x) const {
    assert(&mgr == &x.mgr);
    return x.store == store;
  }
  bool operator!=(const StoreRef &x) const { return !operator==(x); }
  
  ~StoreRef();
  
  Store getStore() const { return store; }
  const StoreManager &getStoreManager() const { return mgr; }
};

}}
#endif
