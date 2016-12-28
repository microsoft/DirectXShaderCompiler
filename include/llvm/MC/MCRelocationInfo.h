//==-- llvm/MC/MCRelocationInfo.h --------------------------------*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCRelocationInfo.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file declares the MCRelocationInfo class, which provides methods to  //
// create MCExprs from relocations, either found in an object::ObjectFile    //
// (object::RelocationRef), or provided through the C API.                   //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_MC_MCRELOCATIONINFO_H
#define LLVM_MC_MCRELOCATIONINFO_H

#include "llvm/Support/Compiler.h"

namespace llvm {

namespace object {
class RelocationRef;
}
class MCExpr;
class MCContext;

/// \brief Create MCExprs from relocations found in an object file.
class MCRelocationInfo {
  MCRelocationInfo(const MCRelocationInfo &) = delete;
  void operator=(const MCRelocationInfo &) = delete;

protected:
  MCContext &Ctx;

public:
  MCRelocationInfo(MCContext &Ctx);
  virtual ~MCRelocationInfo();

  /// \brief Create an MCExpr for the relocation \p Rel.
  /// \returns If possible, an MCExpr corresponding to Rel, else 0.
  virtual const MCExpr *createExprForRelocation(object::RelocationRef Rel);

  /// \brief Create an MCExpr for the target-specific \p VariantKind.
  /// The VariantKinds are defined in llvm-c/Disassembler.h.
  /// Used by MCExternalSymbolizer.
  /// \returns If possible, an MCExpr corresponding to VariantKind, else 0.
  virtual const MCExpr *createExprForCAPIVariantKind(const MCExpr *SubExpr,
                                                     unsigned VariantKind);
};

}

#endif
