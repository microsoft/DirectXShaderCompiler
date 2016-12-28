//===--- FrontendOptions.cpp ----------------------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FrontendOptions.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Frontend/FrontendOptions.h"
#include "llvm/ADT/StringSwitch.h"
using namespace clang;

InputKind FrontendOptions::getInputKindForExtension(StringRef Extension) {
  return llvm::StringSwitch<InputKind>(Extension)
    .Cases("ast", "pcm", IK_AST)
    .Case("c", IK_C)
    .Cases("S", "s", IK_Asm)
    .Case("i", IK_PreprocessedC)
    .Case("ii", IK_PreprocessedCXX)
    .Case("cui", IK_PreprocessedCuda)
    .Case("m", IK_ObjC)
    .Case("mi", IK_PreprocessedObjC)
    .Cases("mm", "M", IK_ObjCXX)
    .Case("mii", IK_PreprocessedObjCXX)
    .Cases("C", "cc", "cp", IK_CXX)
    .Cases("cpp", "CPP", "c++", "cxx", "hpp", IK_CXX)
    .Case("cl", IK_OpenCL)
    .Case("cu", IK_CUDA)
    .Cases("hlsl", "fx", IK_HLSL) // HLSL Change: Inputkind files for hlsl
    .Cases("ll", "bc", IK_LLVM_IR)
    .Default(IK_C);
}
