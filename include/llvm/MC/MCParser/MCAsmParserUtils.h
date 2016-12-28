//===------ llvm/MC/MCAsmParserUtils.h - Asm Parser Utilities ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmParserUtils.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_MC_MCPARSER_MCASMPARSERUTILS_H
#define LLVM_MC_MCPARSER_MCASMPARSERUTILS_H

namespace llvm {

class MCAsmParser;
class MCExpr;
class MCSymbol;
class StringRef;

namespace MCParserUtils {

/// Parse a value expression and return whether it can be assigned to a symbol
/// with the given name.
///
/// On success, returns false and sets the Symbol and Value output parameters.
bool parseAssignmentExpression(StringRef Name, bool allow_redef,
                               MCAsmParser &Parser, MCSymbol *&Symbol,
                               const MCExpr *&Value);

} // namespace MCParserUtils
} // namespace llvm

#endif
