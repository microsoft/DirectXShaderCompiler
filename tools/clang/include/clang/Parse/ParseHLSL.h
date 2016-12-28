//===--- ParseHLSL.h - Standalone HLSL parsing -----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ParseHLSL.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the clang::ParseAST method.                            //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_PARSE_PARSEHLSL_H
#define LLVM_CLANG_PARSE_PARSEHLSL_H

namespace llvm {
class raw_ostream;
}

namespace hlsl {
enum class DxilRootSignatureVersion;
struct DxilVersionedRootSignatureDesc;
}

namespace clang {
class DiagnosticsEngine;

bool ParseHLSLRootSignature(_In_count_(Len) const char *pData, unsigned Len,
                            hlsl::DxilRootSignatureVersion Ver,
                            hlsl::DxilVersionedRootSignatureDesc **ppDesc,
                            clang::SourceLocation Loc,
                            clang::DiagnosticsEngine &Diags);
void ReportHLSLRootSigError(clang::DiagnosticsEngine &Diags,
                            clang::SourceLocation Loc,
                            _In_count_(Len) const char *pData, unsigned Len);
}

#endif
