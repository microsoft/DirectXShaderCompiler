///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcisenseimpl.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Compiler IntelliSense component.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXC_ISENSEIMPL__
#define __DXC_ISENSEIMPL__

#include "clang-c/Index.h"
#include "dxc/dxcisense.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/DxcLangExtensionsHelper.h"

// Forward declarations.
class DxcCursor;
class DxcDiagnostic;
class DxcFile;
class DxcIndex;
class DxcIntelliSense;
class DxcSourceLocation;
class DxcSourceRange;
class DxcTranslationUnit;
class DxcToken;
struct IMalloc;

class DxcCursor : public IDxcCursor
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXCursor m_cursor;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcCursor>(this, iid, ppvObject);
  }

  DxcCursor();
  ~DxcCursor();
  void Initialize(const CXCursor& cursor);
  static HRESULT Create(const CXCursor& cursor, _Outptr_result_nullonfailure_ IDxcCursor** pObject);

  HRESULT STDMETHODCALLTYPE GetExtent(_Outptr_result_nullonfailure_ IDxcSourceRange** pRange) override;
  HRESULT STDMETHODCALLTYPE GetLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** pResult) override;
  HRESULT STDMETHODCALLTYPE GetKind(_Out_ DxcCursorKind* pResult) override;
  HRESULT STDMETHODCALLTYPE GetKindFlags(_Out_ DxcCursorKindFlags* pResult) override;
  HRESULT STDMETHODCALLTYPE GetSemanticParent(_Outptr_result_nullonfailure_ IDxcCursor** pResult) override;
  HRESULT STDMETHODCALLTYPE GetLexicalParent(_Outptr_result_nullonfailure_ IDxcCursor** pResult) override;
  HRESULT STDMETHODCALLTYPE GetCursorType(_Outptr_result_nullonfailure_ IDxcType** pResult) override;
  HRESULT STDMETHODCALLTYPE GetNumArguments(_Out_ int* pResult) override;
  HRESULT STDMETHODCALLTYPE GetArgumentAt(int index, _Outptr_result_nullonfailure_ IDxcCursor** pResult) override;
  HRESULT STDMETHODCALLTYPE GetReferencedCursor(_Outptr_result_nullonfailure_ IDxcCursor** pResult) override;
  HRESULT STDMETHODCALLTYPE GetDefinitionCursor(_Outptr_result_nullonfailure_ IDxcCursor** pResult) override;
  HRESULT STDMETHODCALLTYPE FindReferencesInFile(
    _In_ IDxcFile* file, unsigned skip, unsigned top,
    _Out_ unsigned* pResultLength, _Outptr_result_buffer_maybenull_(*pResultLength) IDxcCursor*** pResult) override;
  HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pResult) override;
  HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcCursor* other, _Out_ BOOL* pResult) override;
  HRESULT STDMETHODCALLTYPE IsNull(_Out_ BOOL* pResult) override;
  HRESULT STDMETHODCALLTYPE IsDefinition(_Out_ BOOL* pResult) override;
  /// <summary>Gets the display name for the cursor, including e.g. parameter types for a function.</summary>
  HRESULT STDMETHODCALLTYPE GetDisplayName(_Outptr_result_maybenull_ BSTR* pResult) override;
  /// <summary>Gets the qualified name for the symbol the cursor refers to.</summary>
  HRESULT STDMETHODCALLTYPE GetQualifiedName(BOOL includeTemplateArgs, _Outptr_result_maybenull_ BSTR* pResult) override;
  /// <summary>Gets a name for the cursor, applying the specified formatting flags.</summary>
  HRESULT STDMETHODCALLTYPE GetFormattedName(DxcCursorFormatting formatting, _Outptr_result_maybenull_ BSTR* pResult) override;
  /// <summary>Gets children in pResult up to top elements.</summary>
  HRESULT STDMETHODCALLTYPE GetChildren(
    unsigned skip, unsigned top,
    _Out_ unsigned* pResultLength, _Outptr_result_buffer_maybenull_(*pResultLength) IDxcCursor*** pResult) override;
  /// <summary>Gets the cursor following a location within a compound cursor.</summary>
  HRESULT STDMETHODCALLTYPE GetSnappedChild(_In_ IDxcSourceLocation* location, _Outptr_result_maybenull_ IDxcCursor** pResult) override;
};

class DxcDiagnostic : public IDxcDiagnostic
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXDiagnostic m_diagnostic;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcDiagnostic>(this, iid, ppvObject);
  }

  DxcDiagnostic();
  ~DxcDiagnostic();
  void Initialize(const CXDiagnostic& diagnostic);
  static HRESULT Create(const CXDiagnostic& diagnostic, _Outptr_result_nullonfailure_ IDxcDiagnostic** pObject);

  HRESULT STDMETHODCALLTYPE FormatDiagnostic(
    DxcDiagnosticDisplayOptions options,
    _Outptr_result_maybenull_ LPSTR* pResult) override;
  HRESULT STDMETHODCALLTYPE GetSeverity(_Out_ DxcDiagnosticSeverity* pResult) override;
  HRESULT STDMETHODCALLTYPE GetLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** pResult) override;
  HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pResult) override;
  HRESULT STDMETHODCALLTYPE GetCategoryText(_Outptr_result_maybenull_ LPSTR* pResult) override;
  HRESULT STDMETHODCALLTYPE GetNumRanges(_Out_ unsigned* pResult) override;
  HRESULT STDMETHODCALLTYPE GetRangeAt(unsigned index, _Outptr_result_nullonfailure_ IDxcSourceRange** pResult) override;
  HRESULT STDMETHODCALLTYPE GetNumFixIts(_Out_ unsigned* pResult) override;
  HRESULT STDMETHODCALLTYPE GetFixItAt(unsigned index,
    _Outptr_result_nullonfailure_ IDxcSourceRange** pReplacementRange, _Outptr_result_maybenull_ LPSTR* pText) override;
};

class DxcFile : public IDxcFile
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXFile m_file;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcFile>(this, iid, ppvObject);
  }

  DxcFile();
  ~DxcFile();
  void Initialize(const CXFile& file);
  static HRESULT Create(const CXFile& file, _Outptr_result_nullonfailure_ IDxcFile** pObject);

  const CXFile& GetFile() const { return m_file; }
  HRESULT STDMETHODCALLTYPE GetName(_Outptr_result_maybenull_ LPSTR* pResult) override;
  HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcFile* other, _Out_ BOOL* pResult) override;
};

class DxcInclusion : public IDxcInclusion
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXFile m_file;
  _Field_size_(m_locationLength)
  CXSourceLocation *m_locations;
  unsigned m_locationLength;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcInclusion>(this, iid, ppvObject);
  }

  DxcInclusion();
  ~DxcInclusion();
  HRESULT Initialize(CXFile file, unsigned locations, _In_count_(locations) CXSourceLocation *pLocation);
  static HRESULT Create(CXFile file, unsigned locations, _In_count_(locations) CXSourceLocation *pLocation, _COM_Outptr_ IDxcInclusion **pResult);

  HRESULT STDMETHODCALLTYPE GetIncludedFile(_Outptr_result_nullonfailure_ IDxcFile** pResult) override;
  HRESULT STDMETHODCALLTYPE GetStackLength(_Out_ unsigned *pResult) override;
  HRESULT STDMETHODCALLTYPE GetStackItem(unsigned index, _Outptr_result_nullonfailure_ IDxcSourceLocation **pResult) override;
};

class DxcIndex : public IDxcIndex
{
private:
    DXC_MICROCOM_TM_REF_FIELDS()
    CXIndex m_index;
    DxcGlobalOptions m_options;
    hlsl::DxcLangExtensionsHelper m_langHelper;
public:
    DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
    {
      return DoBasicQueryInterface<IDxcIndex>(this, iid, ppvObject);
    }

    DxcIndex();
    ~DxcIndex();
    HRESULT Initialize(hlsl::DxcLangExtensionsHelper& langHelper);
    static HRESULT Create(hlsl::DxcLangExtensionsHelper& langHelper, _Outptr_result_nullonfailure_ DxcIndex** index);

    HRESULT STDMETHODCALLTYPE SetGlobalOptions(DxcGlobalOptions options) override;
    HRESULT STDMETHODCALLTYPE GetGlobalOptions(_Out_ DxcGlobalOptions* options) override;
    HRESULT STDMETHODCALLTYPE ParseTranslationUnit(
      _In_z_ const char *source_filename,
      _In_count_(num_command_line_args) const char * const *command_line_args,
      int num_command_line_args,
      _In_count_(num_unsaved_files) IDxcUnsavedFile** unsaved_files,
      unsigned num_unsaved_files,
      DxcTranslationUnitFlags options,
      _Outptr_result_nullonfailure_ IDxcTranslationUnit** pTranslationUnit) override;
};

class DxcIntelliSense : public IDxcIntelliSense, public IDxcLangExtensions {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  hlsl::DxcLangExtensionsHelper m_langHelper;

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL();
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langHelper);

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcIntelliSense, IDxcLangExtensions>(
        this, iid, ppvObject);
  }

  DxcIntelliSense(IMalloc *pMalloc);

    HRESULT STDMETHODCALLTYPE CreateIndex(_Outptr_result_nullonfailure_ IDxcIndex** index) override;
    HRESULT STDMETHODCALLTYPE GetNullLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** location) override;
    HRESULT STDMETHODCALLTYPE GetNullRange(_Outptr_result_nullonfailure_ IDxcSourceRange** location) override;
    HRESULT STDMETHODCALLTYPE GetRange(
      _In_ IDxcSourceLocation* start,
      _In_ IDxcSourceLocation* end,
      _Outptr_result_nullonfailure_ IDxcSourceRange** location) override;
    HRESULT STDMETHODCALLTYPE GetDefaultDiagnosticDisplayOptions(
      _Out_ DxcDiagnosticDisplayOptions* pValue) override;
    HRESULT STDMETHODCALLTYPE GetDefaultEditingTUOptions(_Out_ DxcTranslationUnitFlags* pValue) override;
    HRESULT STDMETHODCALLTYPE CreateUnsavedFile(
      _In_ LPCSTR fileName, _In_ LPCSTR contents, unsigned contentLength,
      _Outptr_result_nullonfailure_ IDxcUnsavedFile** pResult) override;
};

class DxcSourceLocation : public IDxcSourceLocation
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXSourceLocation m_location;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcSourceLocation>(this, iid, ppvObject);
  }

  DxcSourceLocation();
  ~DxcSourceLocation();
  void Initialize(const CXSourceLocation& location);
  static HRESULT Create(const CXSourceLocation& location, _Outptr_result_nullonfailure_ IDxcSourceLocation** pObject);

  const CXSourceLocation& GetLocation() const { return m_location; }
  HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcSourceLocation* other, _Out_ BOOL* pValue) override;
  HRESULT STDMETHODCALLTYPE GetSpellingLocation(
    _Outptr_opt_ IDxcFile** pFile,
    _Out_opt_ unsigned* pLine,
    _Out_opt_ unsigned* pCol,
    _Out_opt_ unsigned* pOffset) override;
  HRESULT STDMETHODCALLTYPE IsNull(_Out_ BOOL* pResult) override;
  HRESULT STDMETHODCALLTYPE GetPresumedLocation(
    _Outptr_opt_ LPSTR* pFilename,
    _Out_opt_ unsigned* pLine,
    _Out_opt_ unsigned* pCol) override;
};

class DxcSourceRange : public IDxcSourceRange
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXSourceRange m_range;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcSourceRange>(this, iid, ppvObject);
  }

  DxcSourceRange();
  ~DxcSourceRange();
  void Initialize(const CXSourceRange& range);
  static HRESULT Create(const CXSourceRange& range, _Outptr_result_nullonfailure_ IDxcSourceRange** pObject);

  const CXSourceRange& GetRange() const { return m_range; }
  HRESULT STDMETHODCALLTYPE IsNull(_Out_ BOOL* pValue) override;
  HRESULT STDMETHODCALLTYPE GetStart(_Outptr_result_nullonfailure_ IDxcSourceLocation** pValue) override;
  HRESULT STDMETHODCALLTYPE GetEnd(_Outptr_result_nullonfailure_ IDxcSourceLocation** pValue) override;
  HRESULT STDMETHODCALLTYPE GetOffsets(_Out_ unsigned* startOffset, _Out_ unsigned* endOffset) override;
};

class DxcToken : public IDxcToken
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXToken m_token;
  CXTranslationUnit m_tu;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcToken>(this, iid, ppvObject);
  }

  DxcToken();
  ~DxcToken();
  void Initialize(const CXTranslationUnit& tu, const CXToken& token);
  static HRESULT Create(const CXTranslationUnit& tu, const CXToken& token, _Outptr_result_nullonfailure_ IDxcToken** pObject);

  HRESULT STDMETHODCALLTYPE GetKind(_Out_ DxcTokenKind* pValue) override;
  HRESULT STDMETHODCALLTYPE GetLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** pValue) override;
  HRESULT STDMETHODCALLTYPE GetExtent(_Outptr_result_nullonfailure_ IDxcSourceRange** pValue) override;
  HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pValue) override;
};

class DxcTranslationUnit : public IDxcTranslationUnit
{
private:
    DXC_MICROCOM_TM_REF_FIELDS()
    CXTranslationUnit m_tu;
public:
    DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
    {
      return DoBasicQueryInterface<IDxcTranslationUnit>(this, iid, ppvObject);
    }

    DxcTranslationUnit();
    ~DxcTranslationUnit();
    void Initialize(CXTranslationUnit tu);

    HRESULT STDMETHODCALLTYPE GetCursor(_Outptr_ IDxcCursor** pCursor) override;
    HRESULT STDMETHODCALLTYPE Tokenize(
      _In_ IDxcSourceRange* range,
      _Outptr_result_buffer_maybenull_(*pTokenCount) IDxcToken*** pTokens,
      _Out_ unsigned* pTokenCount) override;
    HRESULT STDMETHODCALLTYPE GetLocation(
      _In_ IDxcFile* file,
      unsigned line, unsigned column,
      _Outptr_result_nullonfailure_ IDxcSourceLocation** pResult) override;
    HRESULT STDMETHODCALLTYPE GetNumDiagnostics(_Out_ unsigned* pValue) override;
    HRESULT STDMETHODCALLTYPE GetDiagnostic(unsigned index, _Outptr_result_nullonfailure_ IDxcDiagnostic** pValue) override;
    HRESULT STDMETHODCALLTYPE GetFile(_In_ const char* name, _Outptr_result_nullonfailure_ IDxcFile** pResult) override;
    HRESULT STDMETHODCALLTYPE GetFileName(_Outptr_result_maybenull_ LPSTR* pResult) override;
    HRESULT STDMETHODCALLTYPE Reparse(
      _In_count_(num_unsaved_files) IDxcUnsavedFile** unsaved_files,
      unsigned num_unsaved_files) override;
    HRESULT STDMETHODCALLTYPE GetCursorForLocation(_In_ IDxcSourceLocation* location, _Outptr_result_nullonfailure_ IDxcCursor** pResult) override;
    HRESULT STDMETHODCALLTYPE GetLocationForOffset(_In_ IDxcFile* file, unsigned offset, _Outptr_ IDxcSourceLocation** pResult) override;
    HRESULT STDMETHODCALLTYPE GetSkippedRanges(_In_ IDxcFile* file, _Out_ unsigned* pResultCount, _Outptr_result_buffer_(*pResultCount) IDxcSourceRange*** pResult) override;
    HRESULT STDMETHODCALLTYPE GetDiagnosticDetails(unsigned index, DxcDiagnosticDisplayOptions options,
      _Out_ unsigned* errorCode,
      _Out_ unsigned* errorLine,
      _Out_ unsigned* errorColumn,
      _Out_ BSTR* errorFile,
      _Out_ unsigned* errorOffset,
      _Out_ unsigned* errorLength,
      _Out_ BSTR* errorMessage) override;
    HRESULT STDMETHODCALLTYPE GetInclusionList(_Out_ unsigned* pResultCount, _Outptr_result_buffer_(*pResultCount) IDxcInclusion*** pResult) override;
    HRESULT STDMETHODCALLTYPE CodeCompleteAt(
      _In_ char *fileName, unsigned line, unsigned column,
      _In_ IDxcUnsavedFile** pUnsavedFiles, unsigned numUnsavedFiles,
      _In_ DxcCodeCompleteFlags options,
      _Outptr_result_nullonfailure_ IDxcCodeCompleteResults **pResult)
      override;
};

class DxcType : public IDxcType
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXType m_type;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
  {
    return DoBasicQueryInterface<IDxcType>(this, iid, ppvObject);
  }

  DxcType();
  ~DxcType();
  void Initialize(const CXType& type);
  static HRESULT Create(const CXType& type, _Outptr_result_nullonfailure_ IDxcType** pObject);

  HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pResult) override;
  HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcType* other, _Out_ BOOL* pResult) override;
  HRESULT STDMETHODCALLTYPE GetKind(_Out_ DxcTypeKind* pResult) override;
};

class DxcCodeCompleteResults : public IDxcCodeCompleteResults
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXCodeCompleteResults* m_ccr;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcCodeCompleteResults>(this, iid, ppvObject);
  }

  DxcCodeCompleteResults();
  ~DxcCodeCompleteResults();
  void Initialize(CXCodeCompleteResults* ccr);

  HRESULT STDMETHODCALLTYPE GetNumResults(_Out_ unsigned *pResult) override;
  HRESULT STDMETHODCALLTYPE GetResultAt(unsigned index, _Outptr_result_nullonfailure_ IDxcCompletionResult **pResult) override;
};

class DxcCompletionResult : public IDxcCompletionResult
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXCompletionResult m_cr;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcCompletionResult>(this, iid, ppvObject);
  }

  DxcCompletionResult();
  ~DxcCompletionResult();
  void Initialize(const CXCompletionResult& cr);

  HRESULT STDMETHODCALLTYPE GetCursorKind(_Out_ DxcCursorKind *pResult) override;
  HRESULT STDMETHODCALLTYPE GetCompletionString(_Outptr_result_nullonfailure_ IDxcCompletionString **pResult) override;
};

class DxcCompletionString : public IDxcCompletionString
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CXCompletionString m_cs;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcCompletionString>(this, iid, ppvObject);
  }

  DxcCompletionString();
  ~DxcCompletionString();
  void Initialize(const CXCompletionString& cs);

  HRESULT STDMETHODCALLTYPE GetNumCompletionChunks(_Out_ unsigned *pResult) override;
  HRESULT STDMETHODCALLTYPE GetCompletionChunkKind(unsigned chunkNumber, _Out_ DxcCompletionChunkKind *pResult) override;
  HRESULT STDMETHODCALLTYPE GetCompletionChunkText(unsigned chunkNumber, _Out_ LPSTR* pResult) override;
};

HRESULT CreateDxcIntelliSense(_In_ REFIID riid, _Out_ LPVOID* ppv) throw();

#endif
