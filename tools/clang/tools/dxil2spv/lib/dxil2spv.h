///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxil2spv.h                                                                //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides wrappers to dxil2spv main function.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef __DXIL2SPV_DXIL2SPV__
#define __DXIL2SPV_DXIL2SPV__

#include "dxc/dxcapi.h"

namespace llvm {
class raw_ostream;
}

namespace dxil2spvlib {
int RunTranslator(CComPtr<IDxcBlobEncoding> blob, llvm::raw_ostream &OS,
                  llvm::raw_ostream &ERR);
}

#endif // __DXIL2SPV_DXIL2SPV__
