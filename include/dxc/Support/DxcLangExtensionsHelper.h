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
#include "dxc/Support/FileIOHelper.h"
#include <vector>

namespace llvm {
class raw_string_ostream;
class CallInst;
class Value;
}
namespace clang {
class CompilerInstance;
}

namespace hlsl {

class DxcLangExtensionsHelper : public DxcLangExtensionsHelperApply {
private:
  llvm::SmallVector<std::string, 2> m_semanticDefines;
  llvm::SmallVector<std::string, 2> m_semanticDefineExclusions;
  llvm::SmallVector<std::string, 2> m_defines;
  llvm::SmallVector<CComPtr<IDxcIntrinsicTable>, 2> m_intrinsicTables;
  CComPtr<IDxcSemanticDefineValidator> m_semanticDefineValidator;
  std::string m_semanticDefineMetaDataName;

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
  const std::string &GetSemanticDefineMetadataName() { return m_semanticDefineMetaDataName; }

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

  // Set the validator used to validate semantic defines.
  // Only one validator stored and used to run validation.
  HRESULT STDMETHODCALLTYPE SetSemanticDefineValidator(_In_ IDxcSemanticDefineValidator* pValidator) {
    try {
      IFTPTR(pValidator);
      m_semanticDefineValidator = pValidator;
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE SetSemanticDefineMetaDataName(LPCSTR name) {
    m_semanticDefineMetaDataName = name;
    return S_OK;
  }

  // Get the name of the dxil intrinsic function.
  std::string GetIntrinsicName(UINT opcode) {
    LPCSTR pName = "";
    for (IDxcIntrinsicTable *table : m_intrinsicTables) {
      if (SUCCEEDED(table->GetIntrinsicName(opcode, &pName))) {
        return pName;
      }
    }

      return "";
  }

  // Result of validating a semantic define.
  // Stores any warning or error messages produced by the validator.
  // Successful validation means that there are no warning or error messages.
  struct SemanticDefineValidationResult {
    std::string Warning;
    std::string Error;

    bool HasError() { return Error.size() > 0; }
    bool HasWarning() { return Warning.size() > 0; }

    static SemanticDefineValidationResult Success() {
      return SemanticDefineValidationResult();
    }
  };

  // Use the contained semantice define validator to validate the given semantic define.
  SemanticDefineValidationResult ValidateSemanticDefine(const std::string &name, const std::string &value) {
    if (!m_semanticDefineValidator)
      return SemanticDefineValidationResult::Success();

    CComPtr<IDxcBlobEncoding> pError;
    CComPtr<IDxcBlobEncoding> pWarning;
    HRESULT result = m_semanticDefineValidator->GetSemanticDefineWarningsAndErrors(name.c_str(), value.c_str(), &pWarning, &pError);

    if (FAILED(result)) {
      // Hmmm... what to do?
      // Failure indicates it was not able to even run validation so
      // we cannot say the define is invalid. Let's return success and
      // hope for the best.
      return SemanticDefineValidationResult::Success();
    }

    // Function to convert encoded blob into a string.
    auto GetErrorAsString = [&name](const CComPtr<IDxcBlobEncoding> &pBlobString) -> std::string {
      CComPtr<IDxcBlobEncoding> pUTF8BlobStr;
      if (SUCCEEDED(hlsl::DxcGetBlobAsUtf8(pBlobString, &pUTF8BlobStr)))
        return std::string(static_cast<char*>(pUTF8BlobStr->GetBufferPointer()), pUTF8BlobStr->GetBufferSize());
      else
        return std::string("invalid semantic define " + name);
    };

    // Check to see if any warnings or errors were produced.
    std::string error;
    std::string warning;
    if (pError && pError->GetBufferSize()) {
      error = GetErrorAsString(pError);
    }
    if (pWarning && pWarning->GetBufferSize()) {
      warning = GetErrorAsString(pWarning);
    }

    return SemanticDefineValidationResult{ warning, error };
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

  __override DxcLangExtensionsHelper *GetDxcLangExtensionsHelper() {
    return this;
  }
 
  DxcLangExtensionsHelper()
  : m_semanticDefineMetaDataName("hlsl.semdefs")
  {}
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
  __override HRESULT STDMETHODCALLTYPE SetSemanticDefineValidator(_In_ IDxcSemanticDefineValidator* pValidator) { \
    return (_helper_field_).SetSemanticDefineValidator(pValidator); \
  } \
  __override HRESULT STDMETHODCALLTYPE SetSemanticDefineMetaDataName(LPCSTR name) { \
    return (_helper_field_).SetSemanticDefineMetaDataName(name); \
  } \

// A parsed semantic define is a semantic define that has actually been
// parsed by the compiler. It has a name (required), a value (could be
// the empty string), and a location. We use an encoded clang::SourceLocation
// for the location to avoid a clang include dependency.
struct ParsedSemanticDefine{
  std::string Name;
  std::string Value;
  unsigned Location;
};
typedef std::vector<ParsedSemanticDefine> ParsedSemanticDefineList;

// Return the collection of semantic defines parsed by the compiler instance.
ParsedSemanticDefineList
  CollectSemanticDefinesParsedByCompiler(clang::CompilerInstance &compiler,
                                         _In_ DxcLangExtensionsHelper *helper);

} // namespace hlsl

#endif
