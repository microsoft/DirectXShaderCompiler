///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcValidatorBase.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator base validation functionality            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.h"

namespace llvm {
class Module;
}

namespace hlsl {
class AbstractMemoryStream;
}

class DxcValidatorBase : public IDxcValidator2 {
private:
  CComPtr<IMalloc> m_pMalloc;
  HRESULT RunValidation(
      IDxcBlob *pShader,          // Shader to validate.
      UINT32 Flags,               // Validation flags.
      llvm::Module *pModule,      // Module to validate, if available.
      llvm::Module *pDebugModule, // Debug module to validate, if available
      hlsl::AbstractMemoryStream *pDiagStream);

  HRESULT RunRootSignatureValidation(IDxcBlob *pShader, // Shader to validate.
                                     hlsl::AbstractMemoryStream *pDiagStream);

public:
  DxcValidatorBase(IMalloc *pMalloc) : m_pMalloc(pMalloc) {}

  // For internal use only.
  HRESULT ValidateWithOptModules(
      IDxcBlob *pShader,          // Shader to validate.
      UINT32 Flags,               // Validation flags.
      llvm::Module *pModule,      // Module to validate, if available.
      llvm::Module *pDebugModule, // Debug module to validate, if available
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
  );

  // IDxcValidator
  HRESULT STDMETHODCALLTYPE Validate(
      IDxcBlob *pShader, // Shader to validate.
      UINT32 Flags,      // Validation flags.
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcValidator2
  HRESULT STDMETHODCALLTYPE ValidateWithDebug(
      IDxcBlob *pShader,           // Shader to validate.
      UINT32 Flags,                // Validation flags.
      DxcBuffer *pOptDebugBitcode, // Optional debug module bitcode to provide
                                   // line numbers
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;
};
