/*===-- targets.c - tool for testing libLLVM and llvm-c API ---------------===*\
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// targets.c                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the --targets command in llvm-c-test.                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm-c/TargetMachine.h"
#include <stdio.h>

int targets_list(void) {
  LLVMTargetRef t;
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargets();

  for (t = LLVMGetFirstTarget(); t; t = LLVMGetNextTarget(t)) {
    printf("%s", LLVMGetTargetName(t));
    if (LLVMTargetHasJIT(t))
      printf(" (+jit)");
    printf("\n - %s\n", LLVMGetTargetDescription(t));
  }

  return 0;
}
