///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilUtil.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Dxil helper functions.                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
class Type;
class GlobalVariable;
class Function;
class Module;
}

namespace hlsl {

class DxilFieldAnnotation;
class DxilTypeSystem;

namespace dxilutil {
  unsigned
  GetLegacyCBufferFieldElementSize(DxilFieldAnnotation &fieldAnnotation,
                                   llvm::Type *Ty, DxilTypeSystem &typeSys, bool useStrictPrecision);
  llvm::Type *GetArrayEltTy(llvm::Type *Ty);

  bool IsStaticGlobal(llvm::GlobalVariable *GV);
  bool IsSharedMemoryGlobal(llvm::GlobalVariable *GV);
  bool RemoveUnusedFunctions(llvm::Module &M, llvm::Function *EntryFunc,
                             llvm::Function *PatchConstantFunc, bool IsLib);
}

}