//===-- llvm/CodeGen/DAGCombine.h  ------- SelectionDAG Nodes ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DAGCombine.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//

#ifndef LLVM_CODEGEN_DAGCOMBINE_H
#define LLVM_CODEGEN_DAGCOMBINE_H

namespace llvm {

enum CombineLevel {
  BeforeLegalizeTypes,
  AfterLegalizeTypes,
  AfterLegalizeVectorOps,
  AfterLegalizeDAG
};

} // end llvm namespace

#endif
