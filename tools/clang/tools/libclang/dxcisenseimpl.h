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

class DxcCursor : public IDxcCursor
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXCursor m_cursor;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcCursor>(this, iid, ppvObject);
  }

  DxcCursor();
  ~DxcCursor();
  void Initialize(const CXCursor& cursor);
  static HRESULT Create(const CXCursor& cursor, _Outptr_result_nullonfailure_ IDxcCursor** pObject);

  __override HRESULT STDMETHODCALLTYPE GetExtent(_Outptr_result_nullonfailure_ IDxcSourceRange** pRange);
  __override HRESULT STDMETHODCALLTYPE GetLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** pResult);
  __override HRESULT STDMETHODCALLTYPE GetKind(_Out_ DxcCursorKind* pResult);
  __override HRESULT STDMETHODCALLTYPE GetKindFlags(_Out_ DxcCursorKindFlags* pResult);
  __override HRESULT STDMETHODCALLTYPE GetSemanticParent(_Outptr_result_nullonfailure_ IDxcCursor** pResult);
  __override HRESULT STDMETHODCALLTYPE GetLexicalParent(_Outptr_result_nullonfailure_ IDxcCursor** pResult);
  __override HRESULT STDMETHODCALLTYPE GetCursorType(_Outptr_result_nullonfailure_ IDxcType** pResult);
  __override HRESULT STDMETHODCALLTYPE GetNumArguments(_Out_ int* pResult);
  __override HRESULT STDMETHODCALLTYPE GetArgumentAt(int index, _Outptr_result_nullonfailure_ IDxcCursor** pResult);
  __override HRESULT STDMETHODCALLTYPE GetReferencedCursor(_Outptr_result_nullonfailure_ IDxcCursor** pResult);
  __override HRESULT STDMETHODCALLTYPE GetDefinitionCursor(_Outptr_result_nullonfailure_ IDxcCursor** pResult);
  __override HRESULT STDMETHODCALLTYPE FindReferencesInFile(
    _In_ IDxcFile* file, unsigned skip, unsigned top,
    _Out_ unsigned* pResultLength, _Outptr_result_buffer_maybenull_(*pResultLength) IDxcCursor*** pResult);
  __override HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pResult);
  __override HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcCursor* other, _Out_ BOOL* pResult);
  __override HRESULT STDMETHODCALLTYPE IsNull(_Out_ BOOL* pResult);
  __override HRESULT STDMETHODCALLTYPE IsDefinition(_Out_ BOOL* pResult);
  /// <summary>Gets the display name for the cursor, including e.g. parameter types for a function.</summary>
  __override HRESULT STDMETHODCALLTYPE GetDisplayName(_Outptr_result_maybenull_ BSTR* pResult);
  /// <summary>Gets the qualified name for the symbol the cursor refers to.</summary>
  __override HRESULT STDMETHODCALLTYPE GetQualifiedName(BOOL includeTemplateArgs, _Outptr_result_maybenull_ BSTR* pResult);
  /// <summary>Gets a name for the cursor, applying the specified formatting flags.</summary>
  __override HRESULT STDMETHODCALLTYPE GetFormattedName(DxcCursorFormatting formatting, _Outptr_result_maybenull_ BSTR* pResult);
  /// <summary>Gets children in pResult up to top elements.</summary>
  __override HRESULT STDMETHODCALLTYPE GetChildren(
    unsigned skip, unsigned top,
    _Out_ unsigned* pResultLength, _Outptr_result_buffer_maybenull_(*pResultLength) IDxcCursor*** pResult);
  /// <summary>Gets the cursor following a location within a compound cursor.</summary>
  __override HRESULT STDMETHODCALLTYPE GetSnappedChild(_In_ IDxcSourceLocation* location, _Outptr_result_maybenull_ IDxcCursor** pResult);
};

class DxcDiagnostic : public IDxcDiagnostic
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXDiagnostic m_diagnostic;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcDiagnostic>(this, iid, ppvObject);
  }

  DxcDiagnostic();
  ~DxcDiagnostic();
  void Initialize(const CXDiagnostic& diagnostic);
  static HRESULT Create(const CXDiagnostic& diagnostic, _Outptr_result_nullonfailure_ IDxcDiagnostic** pObject);

  __override HRESULT STDMETHODCALLTYPE FormatDiagnostic(
    DxcDiagnosticDisplayOptions options,
    _Outptr_result_maybenull_ LPSTR* pResult);
  __override HRESULT STDMETHODCALLTYPE GetSeverity(_Out_ DxcDiagnosticSeverity* pResult);
  __override HRESULT STDMETHODCALLTYPE GetLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** pResult);
  __override HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pResult);
  __override HRESULT STDMETHODCALLTYPE GetCategoryText(_Outptr_result_maybenull_ LPSTR* pResult);
  __override HRESULT STDMETHODCALLTYPE GetNumRanges(_Out_ unsigned* pResult);
  __override HRESULT STDMETHODCALLTYPE GetRangeAt(unsigned index, _Outptr_result_nullonfailure_ IDxcSourceRange** pResult);
  __override HRESULT STDMETHODCALLTYPE GetNumFixIts(_Out_ unsigned* pResult);
  __override HRESULT STDMETHODCALLTYPE GetFixItAt(unsigned index,
    _Outptr_result_nullonfailure_ IDxcSourceRange** pReplacementRange, _Outptr_result_maybenull_ LPSTR* pText);
};

class DxcFile : public IDxcFile
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXFile m_file;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcFile>(this, iid, ppvObject);
  }

  DxcFile();
  ~DxcFile();
  void Initialize(const CXFile& file);
  static HRESULT Create(const CXFile& file, _Outptr_result_nullonfailure_ IDxcFile** pObject);

  const CXFile& GetFile() const { return m_file; }
  __override HRESULT STDMETHODCALLTYPE GetName(_Outptr_result_maybenull_ LPSTR* pResult);
  __override HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcFile* other, _Out_ BOOL* pResult);
};

class DxcInclusion : public IDxcInclusion
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXFile m_file;
  _Field_size_(m_locationLength)
  CXSourceLocation *m_locations;
  unsigned m_locationLength;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcInclusion>(this, iid, ppvObject);
  }

  DxcInclusion();
  ~DxcInclusion();
  HRESULT Initialize(CXFile file, unsigned locations, _In_count_(locations) CXSourceLocation *pLocation);
  static HRESULT Create(CXFile file, unsigned locations, _In_count_(locations) CXSourceLocation *pLocation, _COM_Outptr_ IDxcInclusion **pResult);

  __override HRESULT STDMETHODCALLTYPE GetIncludedFile(_Outptr_result_nullonfailure_ IDxcFile** pResult);
  __override HRESULT STDMETHODCALLTYPE GetStackLength(_Out_ unsigned *pResult);
  __override HRESULT STDMETHODCALLTYPE GetStackItem(unsigned index, _Outptr_result_nullonfailure_ IDxcSourceLocation **pResult);
};

class DxcIndex : public IDxcIndex
{
private:
    DXC_MICROCOM_REF_FIELD(m_dwRef)
    CXIndex m_index;
    DxcGlobalOptions m_options;
    hlsl::DxcLangExtensionsHelper m_langHelper;
public:
    DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
    {
      return DoBasicQueryInterface<IDxcIndex>(this, iid, ppvObject);
    }

    DxcIndex();
    ~DxcIndex();
    HRESULT Initialize(hlsl::DxcLangExtensionsHelper& langHelper);
    static HRESULT Create(hlsl::DxcLangExtensionsHelper& langHelper, _Outptr_result_nullonfailure_ DxcIndex** index);

    __override HRESULT STDMETHODCALLTYPE SetGlobalOptions(DxcGlobalOptions options);
    __override HRESULT STDMETHODCALLTYPE GetGlobalOptions(_Out_ DxcGlobalOptions* options);
    __override HRESULT STDMETHODCALLTYPE ParseTranslationUnit(
      _In_z_ const char *source_filename,
      _In_count_(num_command_line_args) const char * const *command_line_args,
      int num_command_line_args,
      _In_count_(num_unsaved_files) IDxcUnsavedFile** unsaved_files,
      unsigned num_unsaved_files,
      DxcTranslationUnitFlags options,
      _Outptr_result_nullonfailure_ IDxcTranslationUnit** pTranslationUnit);
};

class DxcIntelliSense : public IDxcIntelliSense, public IDxcLangExtensions {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef);
  hlsl::DxcLangExtensionsHelper m_langHelper;

public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef);
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langHelper);

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface2<IDxcIntelliSense, IDxcLangExtensions>(
        this, iid, ppvObject);
  }

  DxcIntelliSense();

    __override HRESULT STDMETHODCALLTYPE CreateIndex(_Outptr_result_nullonfailure_ IDxcIndex** index);
    __override HRESULT STDMETHODCALLTYPE GetNullLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** location);
    __override HRESULT STDMETHODCALLTYPE GetNullRange(_Outptr_result_nullonfailure_ IDxcSourceRange** location);
    __override HRESULT STDMETHODCALLTYPE GetRange(
      _In_ IDxcSourceLocation* start,
      _In_ IDxcSourceLocation* end,
      _Outptr_result_nullonfailure_ IDxcSourceRange** location);
    __override HRESULT STDMETHODCALLTYPE GetDefaultDiagnosticDisplayOptions(
      _Out_ DxcDiagnosticDisplayOptions* pValue);
    __override HRESULT STDMETHODCALLTYPE GetDefaultEditingTUOptions(_Out_ DxcTranslationUnitFlags* pValue);
    __override HRESULT STDMETHODCALLTYPE CreateUnsavedFile(
      _In_ LPCSTR fileName, _In_ LPCSTR contents, unsigned contentLength,
      _Outptr_result_nullonfailure_ IDxcUnsavedFile** pResult);
};

class DxcSourceLocation : public IDxcSourceLocation
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXSourceLocation m_location;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcSourceLocation>(this, iid, ppvObject);
  }

  DxcSourceLocation();
  ~DxcSourceLocation();
  void Initialize(const CXSourceLocation& location);
  static HRESULT Create(const CXSourceLocation& location, _Outptr_result_nullonfailure_ IDxcSourceLocation** pObject);

  const CXSourceLocation& GetLocation() const { return m_location; }
  __override HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcSourceLocation* other, _Out_ BOOL* pValue);
  __override HRESULT STDMETHODCALLTYPE GetSpellingLocation(
    _Outptr_opt_ IDxcFile** pFile,
    _Out_opt_ unsigned* pLine,
    _Out_opt_ unsigned* pCol,
    _Out_opt_ unsigned* pOffset);
  __override HRESULT STDMETHODCALLTYPE IsNull(_Out_ BOOL* pResult);
};

class DxcSourceRange : public IDxcSourceRange
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXSourceRange m_range;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcSourceRange>(this, iid, ppvObject);
  }

  DxcSourceRange();
  ~DxcSourceRange();
  void Initialize(const CXSourceRange& range);
  static HRESULT Create(const CXSourceRange& range, _Outptr_result_nullonfailure_ IDxcSourceRange** pObject);

  const CXSourceRange& GetRange() const { return m_range; }
  __override HRESULT STDMETHODCALLTYPE IsNull(_Out_ BOOL* pValue);
  __override HRESULT STDMETHODCALLTYPE GetStart(_Outptr_result_nullonfailure_ IDxcSourceLocation** pValue);
  __override HRESULT STDMETHODCALLTYPE GetEnd(_Outptr_result_nullonfailure_ IDxcSourceLocation** pValue);
  __override HRESULT STDMETHODCALLTYPE GetOffsets(_Out_ unsigned* startOffset, _Out_ unsigned* endOffset);
};

class DxcToken : public IDxcToken
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXToken m_token;
  CXTranslationUnit m_tu;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcToken>(this, iid, ppvObject);
  }

  DxcToken();
  ~DxcToken();
  void Initialize(const CXTranslationUnit& tu, const CXToken& token);
  static HRESULT Create(const CXTranslationUnit& tu, const CXToken& token, _Outptr_result_nullonfailure_ IDxcToken** pObject);

  __override HRESULT STDMETHODCALLTYPE GetKind(_Out_ DxcTokenKind* pValue);
  __override HRESULT STDMETHODCALLTYPE GetLocation(_Outptr_result_nullonfailure_ IDxcSourceLocation** pValue);
  __override HRESULT STDMETHODCALLTYPE GetExtent(_Outptr_result_nullonfailure_ IDxcSourceRange** pValue);
  __override HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pValue);
};

class DxcTranslationUnit : public IDxcTranslationUnit
{
private:
    DXC_MICROCOM_REF_FIELD(m_dwRef)
    CXTranslationUnit m_tu;
public:
    DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
    {
      return DoBasicQueryInterface<IDxcTranslationUnit>(this, iid, ppvObject);
    }

    DxcTranslationUnit();
    ~DxcTranslationUnit();
    void Initialize(CXTranslationUnit tu);

    __override HRESULT STDMETHODCALLTYPE GetCursor(_Outptr_ IDxcCursor** pCursor);
    __override HRESULT STDMETHODCALLTYPE Tokenize(
      _In_ IDxcSourceRange* range,
      _Outptr_result_buffer_maybenull_(*pTokenCount) IDxcToken*** pTokens,
      _Out_ unsigned* pTokenCount);
    __override HRESULT STDMETHODCALLTYPE GetLocation(
      _In_ IDxcFile* file,
      unsigned line, unsigned column,
      _Outptr_result_nullonfailure_ IDxcSourceLocation** pResult);
    __override HRESULT STDMETHODCALLTYPE GetNumDiagnostics(_Out_ unsigned* pValue);
    __override HRESULT STDMETHODCALLTYPE GetDiagnostic(unsigned index, _Outptr_result_nullonfailure_ IDxcDiagnostic** pValue);
    __override HRESULT STDMETHODCALLTYPE GetFile(_In_ const char* name, _Outptr_result_nullonfailure_ IDxcFile** pResult);
    __override HRESULT STDMETHODCALLTYPE GetFileName(_Outptr_result_maybenull_ LPSTR* pResult);
    __override HRESULT STDMETHODCALLTYPE Reparse(
      _In_count_(num_unsaved_files) IDxcUnsavedFile** unsaved_files,
      unsigned num_unsaved_files);
    __override HRESULT STDMETHODCALLTYPE GetCursorForLocation(_In_ IDxcSourceLocation* location, _Outptr_result_nullonfailure_ IDxcCursor** pResult);
    __override HRESULT STDMETHODCALLTYPE GetLocationForOffset(_In_ IDxcFile* file, unsigned offset, _Outptr_ IDxcSourceLocation** pResult);
    __override HRESULT STDMETHODCALLTYPE GetSkippedRanges(_In_ IDxcFile* file, _Out_ unsigned* pResultCount, _Outptr_result_buffer_(*pResultCount) IDxcSourceRange*** pResult);
    __override HRESULT STDMETHODCALLTYPE GetDiagnosticDetails(unsigned index, DxcDiagnosticDisplayOptions options,
      _Out_ unsigned* errorCode,
      _Out_ unsigned* errorLine,
      _Out_ unsigned* errorColumn,
      _Out_ BSTR* errorFile,
      _Out_ unsigned* errorOffset,
      _Out_ unsigned* errorLength,
      _Out_ BSTR* errorMessage);
    __override HRESULT STDMETHODCALLTYPE GetInclusionList(_Out_ unsigned* pResultCount, _Outptr_result_buffer_(*pResultCount) IDxcInclusion*** pResult);
};

class DxcType : public IDxcType
{
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CXType m_type;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcType>(this, iid, ppvObject);
  }

  DxcType();
  ~DxcType();
  void Initialize(const CXType& type);
  static HRESULT Create(const CXType& type, _Outptr_result_nullonfailure_ IDxcType** pObject);

  __override HRESULT STDMETHODCALLTYPE GetSpelling(_Outptr_result_maybenull_ LPSTR* pResult);
  __override HRESULT STDMETHODCALLTYPE IsEqualTo(_In_ IDxcType* other, _Out_ BOOL* pResult);
  __override HRESULT STDMETHODCALLTYPE GetKind(_Out_ DxcTypeKind* pResult);
};

HRESULT CreateDxcIntelliSense(_In_ REFIID riid, _Out_ LPVOID* ppv);

#endif
