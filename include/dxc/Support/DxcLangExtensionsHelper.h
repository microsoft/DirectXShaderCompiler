///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcLangExtensionsHelper.h                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Provides a helper class to implement language extensions to HLSL.         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXCLANGEXTENSIONSHELPER_H__
#define __DXCLANGEXTENSIONSHELPER_H__

#include "dxc/Support/Unicode.h"

namespace hlsl {

class DxcLangExtensionsHelper : public DxcLangExtensionsHelperApply {
private:
  llvm::SmallVector<std::string, 2> m_semanticDefines;
  llvm::SmallVector<std::string, 2> m_semanticDefineExclusions;
  llvm::SmallVector<std::string, 2> m_defines;
  llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2> m_intrinsicTables;

  HRESULT STDMETHODCALLTYPE RegisterIntoVector(LPCWSTR name, llvm::SmallVector<std::string, 2>& here)
  {
    try {
      IFTPTR(name);
      std::string s;
      if (!Unicode::UTF16ToUTF8String(name, &s)) {
        throw ::hlsl::Exception(E_INVALIDARG);
      }
      here.push_back(s);
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }

public:
  const llvm::SmallVector<std::string, 2>& GetSemanticDefines() const { return m_semanticDefines; }
  const llvm::SmallVector<std::string, 2>& GetSemanticDefineExclusions() const { return m_semanticDefineExclusions; }
  const llvm::SmallVector<std::string, 2>& GetDefines() const { return m_defines; }
  llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2>& GetIntrinsicTables(){ return m_intrinsicTables; }

  HRESULT STDMETHODCALLTYPE RegisterSemanticDefine(LPCWSTR name)
  {
    return RegisterIntoVector(name, m_semanticDefines);
  }

  HRESULT STDMETHODCALLTYPE RegisterSemanticDefineExclusion(LPCWSTR name)
  {
    return RegisterIntoVector(name, m_semanticDefineExclusions);
  }

  HRESULT STDMETHODCALLTYPE RegisterDefine(LPCWSTR name)
  {
    return RegisterIntoVector(name, m_defines);
  }

  HRESULT STDMETHODCALLTYPE RegisterIntrinsicTable(_In_ IDxcIntrinsicTable* pTable)
  {
    try {
      IFTPTR(pTable);
      LPCSTR tableName = nullptr;
      IFT(pTable->GetTableName(&tableName));
      IFTPTR(tableName);
      IFTARG(strcmp(tableName, "op") != 0);   // "op" is reserved for builtin intrinsics
      for (auto &&table : m_intrinsicTables) {
        LPCSTR otherTableName = nullptr;
        IFT(table->GetTableName(&otherTableName));
        IFTPTR(otherTableName);
        IFTARG(strcmp(tableName, otherTableName) != 0); // Added a duplicate table name
      }
      m_intrinsicTables.push_back(pTable);
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  __override void SetupSema(clang::Sema &S) {
    clang::ExternalASTSource *astSource = S.getASTContext().getExternalSource();
    if (clang::ExternalSemaSource *externalSema =
            llvm::dyn_cast_or_null<clang::ExternalSemaSource>(astSource)) {
      for (auto &&table : m_intrinsicTables) {
        hlsl::RegisterIntrinsicTable(externalSema, table);
      }
    }
  }

  __override void SetupPreprocessorOptions(clang::PreprocessorOptions &PPOpts) {
    for (const auto & define : m_defines) {
      PPOpts.addMacroDef(llvm::StringRef(define.c_str()));
    }
  }
};

// Use this macro to embed an implementation that will delegate to a field.
// Note that QueryInterface still needs to return the vtable.
#define DXC_LANGEXTENSIONS_HELPER_IMPL(_helper_field_) \
  __override HRESULT STDMETHODCALLTYPE RegisterIntrinsicTable(_In_ IDxcIntrinsicTable *pTable) { \
    return (_helper_field_).RegisterIntrinsicTable(pTable); \
  } \
  __override HRESULT STDMETHODCALLTYPE RegisterSemanticDefine(LPCWSTR name) { \
    return (_helper_field_).RegisterSemanticDefine(name); \
  } \
  __override HRESULT STDMETHODCALLTYPE RegisterSemanticDefineExclusion(LPCWSTR name) { \
    return (_helper_field_).RegisterSemanticDefineExclusion(name); \
  } \
  __override HRESULT STDMETHODCALLTYPE RegisterDefine(LPCWSTR name) { \
    return (_helper_field_).RegisterDefine(name); \
  } \

} // namespace hlsl

#endif
