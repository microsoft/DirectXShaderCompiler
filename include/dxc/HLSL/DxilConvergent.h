///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilConvergent.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#pragma once

namespace llvm {
  class Value;
  class Function;
}

namespace hlsl {
  bool IsConvergentMarker(llvm::Value *V);
  llvm::Value *GetConvergentSource(llvm::Value *V);
}
