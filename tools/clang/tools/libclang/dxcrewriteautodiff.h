///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcrewriteautodiff.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Declares the auto-differentiation pass used by the DXC rewriter when      //
// invoked with -generate-differentials.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_TOOLS_LIBCLANG_DXCREWRITEAUTODIFF_H
#define LLVM_CLANG_TOOLS_LIBCLANG_DXCREWRITEAUTODIFF_H

namespace clang {
class TranslationUnitDecl;
struct PrintingPolicy;
} // namespace clang

namespace llvm {
class raw_ostream;
} // namespace llvm

namespace hlsl {

// Print auto-diff variants and, unless GeneratedOnly is set, authored
// declarations. Return false and write a diagnostic to Errors if generated-only
// output would require merging methods into an authored wrapper class.
bool PrintTranslationUnitWithDifferentials(clang::TranslationUnitDecl *tu,
                                           llvm::raw_ostream &OS,
                                           clang::PrintingPolicy &Policy,
                                           bool GeneratedOnly,
                                           llvm::raw_ostream &Errors);

} // namespace hlsl

#endif // LLVM_CLANG_TOOLS_LIBCLANG_DXCREWRITEAUTODIFF_H
