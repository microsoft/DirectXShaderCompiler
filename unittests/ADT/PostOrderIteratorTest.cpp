//===- PostOrderIteratorTest.cpp - PostOrderIterator unit tests -----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PostOrderIteratorTest.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //

#include "gtest/gtest.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
using namespace llvm;
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
namespace {

// Whether we're able to compile
TEST(PostOrderIteratorTest, Compiles) {
  typedef SmallPtrSet<void *, 4> ExtSetTy;

  // Tests that template specializations are kept up to date
  void *Null = nullptr;
  po_iterator_storage<std::set<void *>, false> PIS;
  PIS.insertEdge(Null, Null);
  ExtSetTy Ext;
  po_iterator_storage<ExtSetTy, true> PISExt(Ext);
  PIS.insertEdge(Null, Null);

  // Test above, but going through po_iterator (which inherits from template
  // base)
  BasicBlock *NullBB = nullptr;
  auto PI = po_end(NullBB);
  PI.insertEdge(NullBB, NullBB);
  auto PIExt = po_ext_end(NullBB, Ext);
  PIExt.insertEdge(NullBB, NullBB);
}
}
