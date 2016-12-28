/*===-- object.c - tool for testing libLLVM and llvm-c API ----------------===*\
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// metadata.c                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the --add-named-metadata-operand and --set-metadata  //
// commands in llvm-c-test.                                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm-c-test.h"
#include "llvm-c/Core.h"

int add_named_metadata_operand(void) {
  LLVMModuleRef m = LLVMModuleCreateWithName("Mod");
  LLVMValueRef values[] = { LLVMConstInt(LLVMInt32Type(), 0, 0) };

  // This used to trigger an assertion
  LLVMAddNamedMetadataOperand(m, "name", LLVMMDNode(values, 1));

  LLVMDisposeModule(m);

  return 0;
}

int set_metadata(void) {
  LLVMBuilderRef b = LLVMCreateBuilder();
  LLVMValueRef values[] = { LLVMConstInt(LLVMInt32Type(), 0, 0) };

  // This used to trigger an assertion
  LLVMSetMetadata(
      LLVMBuildRetVoid(b),
      LLVMGetMDKindID("kind", 4),
      LLVMMDNode(values, 1));

  LLVMDisposeBuilder(b);

  return 0;
}
