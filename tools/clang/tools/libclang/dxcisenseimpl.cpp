///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcisenseimpl.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Compiler IntelliSense component.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/Frontend/ASTUnit.h"
#include "llvm/Support/Host.h"
#include "clang/Sema/SemaHLSL.h"

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxcisenseimpl.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"

///////////////////////////////////////////////////////////////////////////////

HRESULT CreateDxcIntelliSense(_In_ REFIID riid, _Out_ LPVOID* ppv) throw()
{
  CComPtr<DxcIntelliSense> isense = CreateOnMalloc<DxcIntelliSense>(DxcGetThreadMallocNoRef());
  if (isense == nullptr)
  {
   *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return isense.p->QueryInterface(riid, ppv);
}

///////////////////////////////////////////////////////////////////////////////

// This is exposed as a helper class, but the implementation works on
// interfaces; we expect callers should be able to use their own.
class DxcBasicUnsavedFile : public IDxcUnsavedFile
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  LPSTR m_fileName;
  LPSTR m_contents;
  unsigned m_length;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject)
  {
    return DoBasicQueryInterface<IDxcUnsavedFile>(this, iid, ppvObject);
  }

  DxcBasicUnsavedFile();
  ~DxcBasicUnsavedFile();
  HRESULT Initialize(_In_z_ LPCSTR fileName, _In_z_ LPCSTR contents, unsigned length);
  static HRESULT Create(_In_z_ LPCSTR fileName, _In_z_ LPCSTR contents, unsigned length, _COM_Outptr_ IDxcUnsavedFile** pObject);

  HRESULT STDMETHODCALLTYPE GetFileName(_Outptr_result_z_ LPSTR* pFileName) override;
  HRESULT STDMETHODCALLTYPE GetContents(_Outptr_result_z_ LPSTR* pContents) override;
  HRESULT STDMETHODCALLTYPE GetLength(_Out_ unsigned* pLength) override;
};

///////////////////////////////////////////////////////////////////////////////

static bool IsCursorKindQualifiedByParent(CXCursorKind kind) throw();

///////////////////////////////////////////////////////////////////////////////

static
HRESULT AnsiToBSTR(_In_opt_z_ const char* text, _Outptr_result_maybenull_ BSTR* pValue) throw()
{
  if (pValue == nullptr) return E_POINTER;
  *pValue = nullptr;
  if (text == nullptr)
  {
    return S_OK;
  }

  //
  // charCount will include the null terminating character, because
  // -1 is used as the input length.
  // SysAllocStringLen takes the character count and adds one for the
  // null terminator, so we remove that from charCount for that call.
  //
  int charCount = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (charCount <= 0)
  {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  *pValue = SysAllocStringLen(nullptr, charCount - 1);
  if (*pValue == nullptr)
  {
    return E_OUTOFMEMORY;
  }

  MultiByteToWideChar(CP_UTF8, 0, text, -1, *pValue, charCount);

  return S_OK;
}

static
HRESULT ConcatPartsReversed(_In_count_(count) CXString* first, int count, _COM_Outptr_ LPSTR* pResult) throw()
{
  HRESULT hr = S_OK;
  size_t total = 0;
  char* writeCursor;

  // Count how big the result should be.
  for (int i = 0; i < count; i++)
  {
    if (i != 0)
    {
      total += 2;
    }
    total += strlen(clang_getCString(first[i]));
  }

  total++; // null terminator

  *pResult = new (std::nothrow) char[total];
  writeCursor = *pResult;
  for (int i = 0; i < count; i++)
  {
    if (i != 0)
    {
      hr = StringCchCopyEx(writeCursor, total, "::", &writeCursor, &total, 0);
      if (FAILED(hr)) break;
    }
    hr = StringCchCopyEx(writeCursor, total, clang_getCString(first[count - 1 - i]), &writeCursor, &total, 0);
    if (FAILED(hr)) break;
  }

  if (FAILED(hr))
  {
    delete[] *pResult;
    *pResult = nullptr;
  }

  return hr;
}

static
_Ret_opt_ _Post_readable_byte_size_(cb)  __drv_allocatesMem(Mem)
LPVOID CoTaskMemAllocZero(SIZE_T cb) throw()
{
  LPVOID result = CoTaskMemAlloc(cb);
  if (result != nullptr)
  {
    ZeroMemory(result, cb);
  }
  return result;
}

/// <summary>Allocates one or more zero-initialized structures in task memory.</summary>
/// <remarks>
/// This does more work than CoTaskMemAlloc, but often simplifies cleanup for
/// error cases.
/// </remarks>
template <typename T>
static
void CoTaskMemAllocZeroElems(_In_ SIZE_T elementCount, _Outptr_result_buffer_maybenull_(elementCount) T**buf) throw()
{
  *buf = reinterpret_cast<T*>(CoTaskMemAllocZero(elementCount * sizeof(T)));
}

static
HRESULT CXStringToAnsiAndDispose(CXString value, _Outptr_result_maybenull_ LPSTR* pValue) throw()
{
  if (pValue == nullptr) return E_POINTER;
  *pValue = nullptr;
  const char* text = clang_getCString(value);
  if (text == nullptr)
  {
    return S_OK;
  }
  size_t len = strlen(text);
  *pValue = (char*)CoTaskMemAlloc(len + 1);
  if (*pValue == nullptr)
  {
    return E_OUTOFMEMORY;
  }
  memcpy(*pValue, text, len + 1);
  clang_disposeString(value);
  return S_OK;
}

static
HRESULT CXStringToBSTRAndDispose(CXString value, _Outptr_result_maybenull_ BSTR* pValue) throw()
{
  HRESULT hr = AnsiToBSTR(clang_getCString(value), pValue);
  clang_disposeString(value);
  return hr;
}

static
void CleanupUnsavedFiles(
  _In_count_(file_count) CXUnsavedFile * files,
  unsigned file_count) throw()
{
  for (unsigned i = 0; i < file_count; ++i)
  {
    CoTaskMemFree((LPVOID)files[i].Filename);
    CoTaskMemFree((LPVOID)files[i].Contents);
  }

  delete[] files;
}

static
HRESULT CoTaskMemAllocString(_In_z_ const char* src, _Outptr_ LPSTR* pResult) throw()
{
  assert(src != nullptr);

  if (pResult == nullptr)
  {
    return E_POINTER;
  }

  unsigned len = strlen(src);
  *pResult = (char*)CoTaskMemAlloc(len + 1);
  if (*pResult == nullptr)
  {
    return E_OUTOFMEMORY;
  }
  CopyMemory(*pResult, src, len + 1);
  return S_OK;
}

static
HRESULT GetCursorQualifiedName(CXCursor cursor, bool includeTemplateArgs, BSTR* pResult) throw()
{
  HRESULT hr = S_OK;
  CSimpleArray<CXString> nameParts;
  char* concatParts = nullptr;
  const CXCursorFormatting DefaultFormatting = (CXCursorFormatting)
    (CXCursorFormatting_SuppressTagKeyword);
  const CXCursorFormatting VariableFormatting = (CXCursorFormatting)
    (CXCursorFormatting_SuppressTagKeyword | CXCursorFormatting_SuppressSpecifiers);

  *pResult = nullptr;

  // To construct the qualifeid name, walk up the semantic parents until we are
  // in the translation unit.
  while (IsCursorKindQualifiedByParent(clang_getCursorKind(cursor)))
  {
    CXCursor parent;
    CXString text;
    text = clang_getCursorSpellingWithFormatting(cursor, DefaultFormatting);
    nameParts.Add(text);
    parent = clang_getCursorSemanticParent(cursor);
    cursor = parent;
  }

  if (nameParts.GetSize() == 0)
  {
    if (clang_getCursorKind(cursor) == CXCursor_VarDecl ||
        (clang_getCursorKind(cursor) == CXCursor_DeclRefExpr &&
         clang_getCursorKind(clang_getCursorReferenced(cursor)) == CXCursor_VarDecl))
    {
      hr = CXStringToBSTRAndDispose(clang_getCursorSpellingWithFormatting(cursor, VariableFormatting), pResult);
    }
    else
    {
      hr = CXStringToBSTRAndDispose(clang_getCursorSpellingWithFormatting(cursor, DefaultFormatting), pResult);
    }
  }
  else if (nameParts.GetSize() == 1)
  {
    hr = CXStringToBSTRAndDispose(nameParts[0], pResult);
    nameParts.RemoveAll();
  }
  else
  {
    hr = ConcatPartsReversed(nameParts.GetData(), nameParts.GetSize(), &concatParts);
    if (SUCCEEDED(hr))
    {
      hr = AnsiToBSTR(concatParts, pResult);
    }
  }

  for (int i = 0; i < nameParts.GetSize(); i++)
  {
    clang_disposeString(nameParts[i]);
  }

  if (concatParts)
  {
    delete[] concatParts;
  }

  return hr;
}

static
bool IsCursorKindQualifiedByParent(CXCursorKind kind) throw()
{
  return
    kind == CXCursor_TypeRef || kind == CXCursor_TemplateRef || kind == CXCursor_NamespaceRef || kind == CXCursor_MemberRef ||
    kind == CXCursor_OverloadedDeclRef ||
    kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl || kind == CXCursor_ClassDecl || kind == CXCursor_EnumDecl ||
    kind == CXCursor_FieldDecl || kind == CXCursor_EnumConstantDecl || kind == CXCursor_FunctionDecl ||
    kind == CXCursor_CXXMethod || kind == CXCursor_Namespace ||
    kind == CXCursor_Constructor || kind == CXCursor_Destructor ||
    kind == CXCursor_FunctionTemplate || kind == CXCursor_ClassTemplate || kind == CXCursor_ClassTemplatePartialSpecialization;
}

template<typename TIface>
static
void SafeReleaseIfaceArray(_Inout_count_(count) TIface** arr, unsigned count) throw()
{
  if (arr != nullptr)
  {
    for (unsigned i = 0; i < count; i++)
    {
      if (arr[i] != nullptr)
      {
        arr[i]->Release();
        arr[i] = nullptr;
      }
    }
  }
}

static
HRESULT SetupUnsavedFiles(
  _In_count_(num_unsaved_files) IDxcUnsavedFile** unsaved_files,
  unsigned num_unsaved_files,
  _Outptr_result_buffer_maybenull_(num_unsaved_files) CXUnsavedFile** files)
{
  *files = nullptr;
  if (num_unsaved_files == 0)
  {
    return S_OK;
  }

  HRESULT hr = S_OK;
  CXUnsavedFile* localFiles = new (std::nothrow) CXUnsavedFile[num_unsaved_files];
  IFROOM(localFiles);
  ZeroMemory(localFiles, num_unsaved_files * sizeof(localFiles[0]));
  for (unsigned i = 0; i < num_unsaved_files; ++i)
  {
    if (unsaved_files[i] == nullptr)
    {
      hr = E_INVALIDARG;
      break;
    }

    hr = unsaved_files[i]->GetFileName((LPSTR*)&localFiles[i].Filename);
    if (FAILED(hr)) break;
    hr = unsaved_files[i]->GetContents((LPSTR*)&localFiles[i].Contents);
    if (FAILED(hr)) break;
    hr = unsaved_files[i]->GetLength((unsigned*)&localFiles[i].Length);
    if (FAILED(hr)) break;
  }

  if (SUCCEEDED(hr))
  {
    *files = localFiles;
  }
  else
  {
    CleanupUnsavedFiles(localFiles, num_unsaved_files);
  }

  return hr;
}

struct PagedCursorVisitorContext
{
  unsigned skip;                // References to skip at the beginning.
  unsigned top;                 // Maximum number of references to get.
  CSimpleArray<CXCursor> refs;  // Cursor references found.
};

static
CXVisitorResult LIBCLANG_CC PagedCursorFindVisit(void *context, CXCursor c, CXSourceRange range)
{
  PagedCursorVisitorContext* pagedContext = (PagedCursorVisitorContext*)context;
  if (pagedContext->skip > 0)
  {
    --pagedContext->skip;
    return CXVisit_Continue;
  }

  pagedContext->refs.Add(c);

  --pagedContext->top;
  return (pagedContext->top == 0) ? CXVisit_Break : CXVisit_Continue;
}

CXChildVisitResult LIBCLANG_CC PagedCursorTraverseVisit(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  PagedCursorVisitorContext* pagedContext = (PagedCursorVisitorContext*)client_data;
  if (pagedContext->skip > 0)
  {
    --pagedContext->skip;
    return CXChildVisit_Continue;
  }

  pagedContext->refs.Add(cursor);

  --pagedContext->top;
  return (pagedContext->top == 0) ? CXChildVisit_Break : CXChildVisit_Continue;
}

static
HRESULT PagedCursorVisitorCopyResults(
  _In_ PagedCursorVisitorContext* context,
  _Out_ unsigned* pResultLength,
  _Outptr_result_buffer_(*pResultLength) IDxcCursor*** pResult)
{
  *pResultLength = 0;
  *pResult = nullptr;

  unsigned resultLength = context->refs.GetSize();
  CoTaskMemAllocZeroElems(resultLength, pResult);
  if (*pResult == nullptr)
  {
    return E_OUTOFMEMORY;
  }

  *pResultLength = resultLength;
  HRESULT hr = S_OK;
  for (unsigned i = 0; i < resultLength; ++i)
  {
    IDxcCursor* newCursor;
    hr = DxcCursor::Create(context->refs[i], &newCursor);
    if (FAILED(hr))
    {
      break;
    }
    (*pResult)[i] = newCursor;
  }

  // Clean up any progress on failure.
  if (FAILED(hr))
  {
    SafeReleaseIfaceArray(*pResult, resultLength);
    CoTaskMemFree(*pResult);
    *pResult = nullptr;
    *pResultLength = 0;
  }

  return hr;
}

struct SourceCursorVisitorContext
{
  const CXSourceLocation& loc;
  CXFile file;
  unsigned offset;

  bool found;
  CXCursor result;
  unsigned resultOffset;
  SourceCursorVisitorContext(const CXSourceLocation& l) : loc(l), found(false)
  {
    clang_getSpellingLocation(loc, &file, nullptr, nullptr, &offset);
  }
};

static
CXChildVisitResult LIBCLANG_CC SourceCursorVisit(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  SourceCursorVisitorContext* context = (SourceCursorVisitorContext*)client_data;

  CXSourceRange range = clang_getCursorExtent(cursor);

  // If the range ends before our location of interest, simply continue; no need
  // to recurse.
  CXFile cursorFile;
  unsigned cursorEndOffset;
  clang_getSpellingLocation(clang_getRangeEnd(range), &cursorFile, nullptr, nullptr, &cursorEndOffset);
  if (cursorFile != context->file || cursorEndOffset < context->offset)
  {
    return CXChildVisit_Continue;
  }

  // If the range start is before or equal to our position, we cover the 
  // location, and we have a result but might need to recurse. If the range 
  // start is after, this is where snapping behavior kicks in, and we'll 
  // consider it just as good as overlap (for the closest match we have). So we 
  // don't in fact need to consider the value, other than to snap to the closest
  // cursor.
  unsigned cursorStartOffset;
  clang_getSpellingLocation(clang_getRangeStart(range), &cursorFile, nullptr, nullptr, &cursorStartOffset);

  bool isKindResult = 
    cursor.kind != CXCursor_CompoundStmt &&
    cursor.kind != CXCursor_TranslationUnit &&
    !clang_isInvalid(cursor.kind);
  if (isKindResult)
  {
    bool cursorIsBetter =
      context->found == false ||
      (cursorStartOffset < context->resultOffset) ||
      (cursorStartOffset == context->resultOffset && cursor.kind == DxcCursor_DeclRefExpr);
    if (cursorIsBetter)
    {
      context->found = true;
      context->result = cursor;
      context->resultOffset = cursorStartOffset;
    }
  }

  return CXChildVisit_Recurse;
}

///////////////////////////////////////////////////////////////////////////////

DxcBasicUnsavedFile::DxcBasicUnsavedFile() : m_fileName(nullptr), m_contents(nullptr)
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcBasicUnsavedFile::~DxcBasicUnsavedFile()
{
  free(m_fileName);
  delete[] m_contents;
}

_Use_decl_annotations_
HRESULT DxcBasicUnsavedFile::Initialize(LPCSTR fileName, LPCSTR contents, unsigned contentLength)
{
  if (fileName == nullptr) return E_INVALIDARG;
  if (contents == nullptr) return E_INVALIDARG;

  m_fileName = _strdup(fileName);
  if (m_fileName == nullptr) return E_OUTOFMEMORY;

  unsigned bufferLength = strlen(contents);
  if (contentLength > bufferLength)
  {
    contentLength = bufferLength;
  }

  m_contents = new (std::nothrow)char[contentLength + 1];
  if (m_contents == nullptr)
  {
    free(m_fileName);
    m_fileName = nullptr;
    return E_OUTOFMEMORY;
  }
  CopyMemory(m_contents, contents, contentLength);
  m_contents[contentLength] = '\0';
  m_length = contentLength;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcBasicUnsavedFile::Create(
  LPCSTR fileName, LPCSTR contents, unsigned contentLength,
  IDxcUnsavedFile** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcBasicUnsavedFile* newValue = new (std::nothrow)DxcBasicUnsavedFile();
  if (newValue == nullptr) return E_OUTOFMEMORY;
  HRESULT hr = newValue->Initialize(fileName, contents, contentLength);
  if (FAILED(hr))
  {
    delete newValue;
    return hr;
  }
  newValue->AddRef();
  *pObject = newValue;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcBasicUnsavedFile::GetFileName(LPSTR* pFileName)
{
  return CoTaskMemAllocString(m_fileName, pFileName);
}

_Use_decl_annotations_
HRESULT DxcBasicUnsavedFile::GetContents(LPSTR* pContents)
{
  return CoTaskMemAllocString(m_contents, pContents);
}

HRESULT DxcBasicUnsavedFile::GetLength(unsigned* pLength)
{
  if (pLength == nullptr) return E_POINTER;
  *pLength = m_length;
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

DxcCursor::DxcCursor()
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcCursor::~DxcCursor()
{
}

_Use_decl_annotations_
HRESULT DxcCursor::Create(const CXCursor& cursor, IDxcCursor** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcCursor* newValue = new (std::nothrow)DxcCursor();
  if (newValue == nullptr) return E_OUTOFMEMORY;
  newValue->Initialize(cursor);
  newValue->AddRef();
  *pObject = newValue;
  return S_OK;
}

void DxcCursor::Initialize(const CXCursor& cursor)
{
  m_cursor = cursor;
}

_Use_decl_annotations_
HRESULT DxcCursor::GetExtent(IDxcSourceRange** pValue)
{
  DxcThreadMalloc TM(m_pMalloc);
  CXSourceRange range = clang_getCursorExtent(m_cursor);
  return DxcSourceRange::Create(range, pValue);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetLocation(IDxcSourceLocation** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(clang_getCursorLocation(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetKind(DxcCursorKind* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = (DxcCursorKind)clang_getCursorKind(m_cursor);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcCursor::GetKindFlags(DxcCursorKindFlags* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  DxcCursorKindFlags f = DxcCursorKind_None;
  CXCursorKind kind = clang_getCursorKind(m_cursor);
  if (0 != clang_isDeclaration(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Declaration);
  if (0 != clang_isReference(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Reference);
  if (0 != clang_isExpression(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Expression);
  if (0 != clang_isStatement(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Statement);
  if (0 != clang_isAttribute(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Attribute);
  if (0 != clang_isInvalid(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Invalid);
  if (0 != clang_isTranslationUnit(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_TranslationUnit);
  if (0 != clang_isPreprocessing(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Preprocessing);
  if (0 != clang_isUnexposed(kind)) f = (DxcCursorKindFlags)(f | DxcCursorKind_Unexposed);
  *pResult = f;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcCursor::GetSemanticParent(IDxcCursor** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCursor::Create(clang_getCursorSemanticParent(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetLexicalParent(IDxcCursor** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCursor::Create(clang_getCursorLexicalParent(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetCursorType(IDxcType** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcType::Create(clang_getCursorType(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetNumArguments(int* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = clang_Cursor_getNumArguments(m_cursor);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcCursor::GetArgumentAt(int index, IDxcCursor** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCursor::Create(clang_Cursor_getArgument(m_cursor, index), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetReferencedCursor(IDxcCursor** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCursor::Create(clang_getCursorReferenced(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetDefinitionCursor(IDxcCursor** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCursor::Create(clang_getCursorDefinition(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::FindReferencesInFile(
  IDxcFile* file, unsigned skip, unsigned top,
  unsigned* pResultLength, IDxcCursor*** pResult)
{
  if (pResultLength == nullptr) return E_POINTER;
  if (pResult == nullptr) return E_POINTER;
  if (file == nullptr) return E_INVALIDARG;

  *pResult = nullptr;
  *pResultLength = 0;
  if (top == 0)
  {
    return S_OK;
  }

  DxcThreadMalloc TM(m_pMalloc);
  CXCursorAndRangeVisitor visitor;
  PagedCursorVisitorContext findReferencesInFileContext;
  findReferencesInFileContext.skip = skip;
  findReferencesInFileContext.top = top;
  visitor.context = &findReferencesInFileContext;
  visitor.visit = PagedCursorFindVisit;

  DxcFile* fileImpl = reinterpret_cast<DxcFile*>(file);
  clang_findReferencesInFile(m_cursor, fileImpl->GetFile(), visitor); // known visitor, so ignore result

  return PagedCursorVisitorCopyResults(&findReferencesInFileContext, pResultLength, pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetSpelling(LPSTR* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToAnsiAndDispose(clang_getCursorSpelling(m_cursor), pResult);
}

HRESULT DxcCursor::IsEqualTo(_In_ IDxcCursor* other, _Out_ BOOL* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  if (other == nullptr)
  {
    *pResult = FALSE;
  }
  else
  {
    DxcCursor* otherImpl = reinterpret_cast<DxcCursor*>(other);
    *pResult = 0 != clang_equalCursors(m_cursor, otherImpl->m_cursor);
  }
  return S_OK;
}

HRESULT DxcCursor::IsNull(_Out_ BOOL* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = 0 != clang_Cursor_isNull(m_cursor);
  return S_OK;
}

HRESULT DxcCursor::IsDefinition(_Out_ BOOL* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = 0 != clang_isCursorDefinition(m_cursor);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcCursor::GetDisplayName(BSTR* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToBSTRAndDispose(clang_getCursorDisplayName(m_cursor), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetQualifiedName(BOOL includeTemplateArgs, BSTR* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return GetCursorQualifiedName(m_cursor, includeTemplateArgs, pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetFormattedName(DxcCursorFormatting formatting, BSTR* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToBSTRAndDispose(clang_getCursorSpellingWithFormatting(m_cursor, formatting), pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetChildren(
  unsigned skip, unsigned top,
  unsigned* pResultLength, IDxcCursor*** pResult)
{
  if (pResultLength == nullptr) return E_POINTER;
  if (pResult == nullptr) return E_POINTER;

  *pResult = nullptr;
  *pResultLength = 0;
  if (top == 0)
  {
    return S_OK;
  }

  DxcThreadMalloc TM(m_pMalloc);
  PagedCursorVisitorContext visitorContext;
  visitorContext.skip = skip;
  visitorContext.top = top;
  clang_visitChildren(m_cursor, PagedCursorTraverseVisit, &visitorContext); // known visitor, so ignore result
  return PagedCursorVisitorCopyResults(&visitorContext, pResultLength, pResult);
}

_Use_decl_annotations_
HRESULT DxcCursor::GetSnappedChild(IDxcSourceLocation* location, IDxcCursor** pResult)
{
  if (location == nullptr) return E_POINTER;
  if (pResult == nullptr) return E_POINTER;

  *pResult = nullptr;

  DxcThreadMalloc TM(m_pMalloc);
  DxcSourceLocation* locationImpl = reinterpret_cast<DxcSourceLocation*>(location);
  const CXSourceLocation& snapLocation = locationImpl->GetLocation();
  SourceCursorVisitorContext visitorContext(snapLocation);
  clang_visitChildren(m_cursor, SourceCursorVisit, &visitorContext);
  if (visitorContext.found)
  {
    return DxcCursor::Create(visitorContext.result, pResult);
  }

  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

DxcDiagnostic::DxcDiagnostic() : m_diagnostic(nullptr)
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcDiagnostic::~DxcDiagnostic()
{
  if (m_diagnostic)
  {
    clang_disposeDiagnostic(m_diagnostic);
  }
}

void DxcDiagnostic::Initialize(const CXDiagnostic& diagnostic)
{
  m_diagnostic = diagnostic;
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::Create(const CXDiagnostic& diagnostic, IDxcDiagnostic** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcDiagnostic* newValue = new (std::nothrow) DxcDiagnostic();
  if (newValue == nullptr) return E_OUTOFMEMORY;
  newValue->Initialize(diagnostic);
  newValue->AddRef();
  *pObject = newValue;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::FormatDiagnostic(
  DxcDiagnosticDisplayOptions options,
  LPSTR* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToAnsiAndDispose(clang_formatDiagnostic(m_diagnostic, options), pResult);
}

HRESULT DxcDiagnostic::GetSeverity(_Out_ DxcDiagnosticSeverity* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = (DxcDiagnosticSeverity)clang_getDiagnosticSeverity(m_diagnostic);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::GetLocation(IDxcSourceLocation** pResult)
{
  return DxcSourceLocation::Create(clang_getDiagnosticLocation(m_diagnostic), pResult);
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::GetSpelling(LPSTR* pResult)
{
  return CXStringToAnsiAndDispose(clang_getDiagnosticSpelling(m_diagnostic), pResult);
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::GetCategoryText(LPSTR* pResult)
{
  return CXStringToAnsiAndDispose(clang_getDiagnosticCategoryText(m_diagnostic), pResult);
}

HRESULT DxcDiagnostic::GetNumRanges(_Out_ unsigned* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = clang_getDiagnosticNumRanges(m_diagnostic);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::GetRangeAt(unsigned index, IDxcSourceRange** pResult)
{
  return DxcSourceRange::Create(clang_getDiagnosticRange(m_diagnostic, index), pResult);
}

HRESULT DxcDiagnostic::GetNumFixIts(_Out_ unsigned* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = clang_getDiagnosticNumFixIts(m_diagnostic);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcDiagnostic::GetFixItAt(unsigned index,
  IDxcSourceRange** pReplacementRange, LPSTR* pText)
{
  if (pReplacementRange == nullptr) return E_POINTER;
  if (pText == nullptr) return E_POINTER;
  *pReplacementRange = nullptr;
  *pText = nullptr;

  DxcThreadMalloc TM(m_pMalloc);
  CXSourceRange range;
  CXString text = clang_getDiagnosticFixIt(m_diagnostic, index, &range);
  HRESULT hr = DxcSourceRange::Create(range, pReplacementRange);
  if (SUCCEEDED(hr))
  {
    hr = CXStringToAnsiAndDispose(text, pText);
    if (FAILED(hr))
    {
      (*pReplacementRange)->Release();
      *pReplacementRange = nullptr;
    }
  }

  return hr;
}

///////////////////////////////////////////////////////////////////////////////

DxcFile::DxcFile()
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcFile::~DxcFile()
{
}

void DxcFile::Initialize(const CXFile& file)
{
  m_file = file;
}

_Use_decl_annotations_
HRESULT DxcFile::Create(const CXFile& file, IDxcFile** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcFile* newValue = new (std::nothrow)DxcFile();
  if (newValue == nullptr) return E_OUTOFMEMORY;
  newValue->Initialize(file);
  newValue->AddRef();
  *pObject = newValue;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcFile::GetName(LPSTR* pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToAnsiAndDispose(clang_getFileName(m_file), pResult);
}

_Use_decl_annotations_
HRESULT DxcFile::IsEqualTo(IDxcFile* other, BOOL* pResult)
{
  if (!pResult) return E_POINTER;
  if (other == nullptr)
  {
    *pResult = FALSE;
  }
  else
  {
    // CXFile is an internal pointer into the source manager, and so
    // should be equal for the same file.
    DxcFile* otherImpl = reinterpret_cast<DxcFile*>(other);
    *pResult = (m_file == otherImpl->m_file) ? TRUE : FALSE;
  }
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

DxcInclusion::DxcInclusion()
    : m_file(nullptr), m_locationLength(0), m_locations(nullptr) {
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcInclusion::~DxcInclusion() {
  delete[] m_locations;
}

_Use_decl_annotations_
HRESULT DxcInclusion::Initialize(CXFile file, unsigned locations, CXSourceLocation *pLocations) {
  if (locations) {
    m_locations = new (std::nothrow)CXSourceLocation[locations];
    if (m_locations == nullptr)
      return E_OUTOFMEMORY;
    std::copy(pLocations, pLocations + locations, m_locations);
    m_locationLength = locations;
  }
  m_file = file;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcInclusion::Create(CXFile file, unsigned locations, CXSourceLocation *pLocations, IDxcInclusion **pResult) {
  if (pResult == nullptr) return E_POINTER;
  *pResult = nullptr;

  CComPtr<DxcInclusion> local;
  local = new (std::nothrow)DxcInclusion();
  if (local == nullptr) return E_OUTOFMEMORY;
  HRESULT hr = local->Initialize(file, locations, pLocations);
  if (FAILED(hr)) return hr;
  *pResult = local.Detach();
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcInclusion::GetIncludedFile(_Outptr_result_nullonfailure_ IDxcFile** pResult) {
  DxcThreadMalloc TM(m_pMalloc);
  return DxcFile::Create(m_file, pResult);
}

_Use_decl_annotations_
HRESULT DxcInclusion::GetStackLength(_Out_ unsigned *pResult) {
  if (pResult == nullptr) return E_POINTER;
  *pResult = m_locationLength;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcInclusion::GetStackItem(unsigned index, _Outptr_result_nullonfailure_ IDxcSourceLocation **pResult) {
  if (pResult == nullptr) return E_POINTER;
  if (index >= m_locationLength) return E_INVALIDARG;
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(m_locations[index], pResult);
}

///////////////////////////////////////////////////////////////////////////////

DxcIndex::DxcIndex() : m_index(0), m_options(DxcGlobalOpt_None)
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcIndex::~DxcIndex()
{
    if (m_index)
    {
        clang_disposeIndex(m_index);
        m_index = 0;
    }
}

HRESULT DxcIndex::Initialize(hlsl::DxcLangExtensionsHelper &langHelper) {
  try {
    m_langHelper = langHelper; // Clone the object.
    m_index = clang_createIndex(1, 0);
    if (m_index == 0) {
      return E_FAIL;
    }

    hlsl::DxcLangExtensionsHelperApply* apply = &m_langHelper;
    clang_index_setLangHelper(m_index, apply);
  }
  CATCH_CPP_RETURN_HRESULT();
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcIndex::Create(hlsl::DxcLangExtensionsHelper &langHelper, DxcIndex** index)
{
  if (index == nullptr) return E_POINTER;
  *index = nullptr;

  CComPtr<DxcIndex> local;
  local = new (std::nothrow) DxcIndex();
  if (local == nullptr) return E_OUTOFMEMORY;
  HRESULT hr = local->Initialize(langHelper);
  if (FAILED(hr)) return hr;
  *index = local.Detach();
  return S_OK;
}

HRESULT DxcIndex::SetGlobalOptions(DxcGlobalOptions options)
{
  m_options = options;
  return S_OK;
}

HRESULT DxcIndex::GetGlobalOptions(_Out_ DxcGlobalOptions* options)
{
  if (options == nullptr) return E_POINTER;
  *options = m_options;
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

_Use_decl_annotations_
HRESULT DxcIndex::ParseTranslationUnit(
  const char *source_filename,
  const char * const *command_line_args,
  int num_command_line_args,
  IDxcUnsavedFile** unsaved_files,
  unsigned num_unsaved_files,
  DxcTranslationUnitFlags options,
  IDxcTranslationUnit** pTranslationUnit)
{
  if (pTranslationUnit == nullptr) return E_POINTER;
  *pTranslationUnit = nullptr;

  if (m_index == 0) return E_FAIL;

  DxcThreadMalloc TM(m_pMalloc);

  CXUnsavedFile* files;
  HRESULT hr = SetupUnsavedFiles(unsaved_files, num_unsaved_files, &files);
  if (FAILED(hr)) return hr;

  try
  {
    // TODO: until an interface to file access is defined and implemented, simply fall back to pure Win32/CRT calls.
    ::llvm::sys::fs::MSFileSystem* msfPtr;
    IFT(CreateMSFileSystemForDisk(&msfPtr));
    std::auto_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());
    CXTranslationUnit tu = clang_parseTranslationUnit(m_index, source_filename,
      command_line_args, num_command_line_args,
      files, num_unsaved_files, options);
    CleanupUnsavedFiles(files, num_unsaved_files);
    if (tu == nullptr)
    {
      return E_FAIL;
    }

    CComPtr<DxcTranslationUnit> localTU = new (std::nothrow) DxcTranslationUnit();
    if (localTU == nullptr)
    {
      clang_disposeTranslationUnit(tu);
      return E_OUTOFMEMORY;
    }
    localTU->Initialize(tu);
    *pTranslationUnit = localTU.Detach();

    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}

///////////////////////////////////////////////////////////////////////////////

DxcIntelliSense::DxcIntelliSense(IMalloc *pMalloc)
{
  m_pMalloc = pMalloc;
}

_Use_decl_annotations_
HRESULT DxcIntelliSense::CreateIndex(IDxcIndex** index)
{
  DxcThreadMalloc TM(m_pMalloc);
  CComPtr<DxcIndex> local;
  HRESULT hr = DxcIndex::Create(m_langHelper, &local);
  *index = local.Detach();
  return hr;
}

_Use_decl_annotations_
HRESULT DxcIntelliSense::GetNullLocation(IDxcSourceLocation** location)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(clang_getNullLocation(), location);
}

_Use_decl_annotations_
HRESULT DxcIntelliSense::GetNullRange(IDxcSourceRange** location)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceRange::Create(clang_getNullRange(), location);
}

_Use_decl_annotations_
HRESULT DxcIntelliSense::GetRange(
  IDxcSourceLocation* start,
  IDxcSourceLocation* end,
  IDxcSourceRange** pResult)
{
  if (start == nullptr || end == nullptr) return E_INVALIDARG;
  if (pResult == nullptr) return E_POINTER;
  DxcSourceLocation* startImpl = reinterpret_cast<DxcSourceLocation*>(start);
  DxcSourceLocation* endImpl = reinterpret_cast<DxcSourceLocation*>(end);
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceRange::Create(
    clang_getRange(startImpl->GetLocation(), endImpl->GetLocation()),
    pResult);
}

HRESULT DxcIntelliSense::GetDefaultDiagnosticDisplayOptions(
  _Out_ DxcDiagnosticDisplayOptions* pValue)
{
  if (pValue == nullptr) return E_POINTER;
  *pValue = (DxcDiagnosticDisplayOptions)clang_defaultDiagnosticDisplayOptions();
  return S_OK;
}

HRESULT DxcIntelliSense::GetDefaultEditingTUOptions(_Out_ DxcTranslationUnitFlags* pValue)
{
  if (pValue == nullptr) return E_POINTER;
  *pValue = (DxcTranslationUnitFlags)
    (clang_defaultEditingTranslationUnitOptions() | (unsigned)DxcTranslationUnitFlags_UseCallerThread);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcIntelliSense::CreateUnsavedFile(LPCSTR fileName, LPCSTR contents, unsigned contentLength, IDxcUnsavedFile** pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return DxcBasicUnsavedFile::Create(fileName, contents, contentLength, pResult);
}

///////////////////////////////////////////////////////////////////////////////

DxcSourceLocation::DxcSourceLocation()
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcSourceLocation::~DxcSourceLocation()
{
}

void DxcSourceLocation::Initialize(const CXSourceLocation& location)
{
  m_location = location;
}

_Use_decl_annotations_
HRESULT DxcSourceLocation::Create(
  const CXSourceLocation& location,
  IDxcSourceLocation** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcSourceLocation* local = new (std::nothrow)DxcSourceLocation();
  if (local == nullptr) return E_OUTOFMEMORY;
  local->Initialize(location);
  local->AddRef();
  *pObject = local;
  return S_OK;
}

HRESULT DxcSourceLocation::IsEqualTo(_In_ IDxcSourceLocation* other, _Out_ BOOL* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  if (other == nullptr)
  {
    *pResult = FALSE;
  }
  else
  {
    DxcSourceLocation* otherImpl = reinterpret_cast<DxcSourceLocation*>(other);
    *pResult = clang_equalLocations(m_location, otherImpl->m_location) != 0;
  }

  return S_OK;
}

HRESULT DxcSourceLocation::GetSpellingLocation(
  _Outptr_opt_ IDxcFile** pFile,
  _Out_opt_ unsigned* pLine,
  _Out_opt_ unsigned* pCol,
  _Out_opt_ unsigned* pOffset)
{
  CXFile file;
  unsigned line, col, offset;
  DxcThreadMalloc TM(m_pMalloc);
  clang_getSpellingLocation(m_location, &file, &line, &col, &offset);
  if (pFile != nullptr)
  {
    HRESULT hr = DxcFile::Create(file, pFile);
    if (FAILED(hr)) return hr;
  }
  if (pLine) *pLine = line;
  if (pCol) *pCol = col;
  if (pOffset) *pOffset = offset;
  return S_OK;
}

HRESULT DxcSourceLocation::IsNull(_Out_ BOOL* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  CXSourceLocation nullLocation = clang_getNullLocation();
  *pResult = 0 !=clang_equalLocations(nullLocation, m_location);
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

DxcSourceRange::DxcSourceRange()
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcSourceRange::~DxcSourceRange()
{
}

void DxcSourceRange::Initialize(const CXSourceRange& range)
{
  m_range = range;
}

_Use_decl_annotations_
HRESULT DxcSourceRange::Create(const CXSourceRange& range, IDxcSourceRange** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcSourceRange* local = new (std::nothrow)DxcSourceRange();
  if (local == nullptr) return E_OUTOFMEMORY;
  local->Initialize(range);
  local->AddRef();
  *pObject = local;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcSourceRange::IsNull(BOOL* pValue)
{
  if (pValue == nullptr) return E_POINTER;
  *pValue = clang_Range_isNull(m_range) != 0;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcSourceRange::GetStart(IDxcSourceLocation** pValue)
{
  CXSourceLocation location = clang_getRangeStart(m_range);
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(location, pValue);
}

_Use_decl_annotations_
HRESULT DxcSourceRange::GetEnd(IDxcSourceLocation** pValue)
{
  CXSourceLocation location = clang_getRangeEnd(m_range);
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(location, pValue);
}

HRESULT DxcSourceRange::GetOffsets(_Out_ unsigned* startOffset, _Out_ unsigned* endOffset)
{
  if (startOffset == nullptr) return E_POINTER;
  if (endOffset == nullptr) return E_POINTER;
  CXSourceLocation startLocation = clang_getRangeStart(m_range);
  CXSourceLocation endLocation = clang_getRangeEnd(m_range);

  CXFile file;
  unsigned line, col;
  clang_getSpellingLocation(startLocation, &file, &line, &col, startOffset);
  clang_getSpellingLocation(endLocation, &file, &line, &col, endOffset);

  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

DxcToken::DxcToken()
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcToken::~DxcToken()
{
}

void DxcToken::Initialize(const CXTranslationUnit& tu, const CXToken& token)
{
  m_tu = tu;
  m_token = token;
}

_Use_decl_annotations_
HRESULT DxcToken::Create(
  const CXTranslationUnit& tu,
  const CXToken& token,
  IDxcToken** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcToken* local = new (std::nothrow)DxcToken();
  if (local == nullptr) return E_OUTOFMEMORY;
  local->Initialize(tu, token);
  local->AddRef();
  *pObject = local;
  return S_OK;
}

HRESULT DxcToken::GetKind(_Out_ DxcTokenKind* pValue)
{
  if (pValue == nullptr) return E_POINTER;
  switch (clang_getTokenKind(m_token))
  {
  case CXToken_Punctuation: *pValue = DxcTokenKind_Punctuation; break;
  case CXToken_Keyword: *pValue = DxcTokenKind_Keyword; break;
  case CXToken_Identifier: *pValue = DxcTokenKind_Identifier; break;
  case CXToken_Literal: *pValue = DxcTokenKind_Literal; break;
  case CXToken_Comment: *pValue = DxcTokenKind_Comment; break;
  case CXToken_BuiltInType: *pValue = DxcTokenKind_BuiltInType; break;
  default: *pValue = DxcTokenKind_Unknown; break;
  }
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcToken::GetLocation(IDxcSourceLocation** pValue)
{
  if (pValue == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(clang_getTokenLocation(m_tu, m_token), pValue);
}

_Use_decl_annotations_
HRESULT DxcToken::GetExtent(IDxcSourceRange** pValue)
{
  if (pValue == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceRange::Create(clang_getTokenExtent(m_tu, m_token), pValue);
}

_Use_decl_annotations_
HRESULT DxcToken::GetSpelling(LPSTR* pValue)
{
  if (pValue == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToAnsiAndDispose(clang_getTokenSpelling(m_tu, m_token), pValue);
}

///////////////////////////////////////////////////////////////////////////////

DxcTranslationUnit::DxcTranslationUnit() : m_tu(nullptr)
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcTranslationUnit::~DxcTranslationUnit() {
  if (m_tu != nullptr) {
    // TODO: until an interface to file access is defined and implemented, simply fall back to pure Win32/CRT calls.
    // Also, note that this can throw / fail in a destructor, which is a big no-no.
    ::llvm::sys::fs::MSFileSystem* msfPtr;
    CreateMSFileSystemForDisk(&msfPtr);
    assert(msfPtr != nullptr);
    std::auto_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    assert(!pts.error_code());

    clang_disposeTranslationUnit(m_tu);
    m_tu = nullptr;
  }
}

void DxcTranslationUnit::Initialize(CXTranslationUnit tu)
{
  m_tu = tu;
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetCursor(IDxcCursor** pCursor)
{
  DxcThreadMalloc TM(m_pMalloc);
  if (m_tu == nullptr) return E_FAIL;
  return DxcCursor::Create(clang_getTranslationUnitCursor(m_tu), pCursor);
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::Tokenize(
  IDxcSourceRange* range,
  IDxcToken*** pTokens,
  unsigned* pTokenCount)
{
  if (range == nullptr) return E_INVALIDARG;
  if (pTokens == nullptr) return E_POINTER;
  if (pTokenCount == nullptr) return E_POINTER;

  *pTokens = nullptr;
  *pTokenCount = 0;

  // Only accept our own source range.
  DxcThreadMalloc TM(m_pMalloc);
  HRESULT hr = S_OK;
  DxcSourceRange* rangeImpl = reinterpret_cast<DxcSourceRange*>(range);
  IDxcToken** localTokens = nullptr;
  CXToken* tokens = nullptr;
  unsigned numTokens = 0;
  clang_tokenize(m_tu, rangeImpl->GetRange(), &tokens, &numTokens);
  if (numTokens != 0)
  {
    CoTaskMemAllocZeroElems(numTokens, &localTokens);
    if (localTokens == nullptr)
    {
      hr = E_OUTOFMEMORY;
    }
    else
    {
      for (unsigned i = 0; i < numTokens; ++i)
      {
        hr = DxcToken::Create(m_tu, tokens[i], &localTokens[i]);
        if (FAILED(hr)) break;
      }
      *pTokens = localTokens;
      *pTokenCount = numTokens;
    }
  }

  // Cleanup partial progress on failures.
  if (FAILED(hr))
  {
    SafeReleaseIfaceArray(localTokens, numTokens);
    delete[] localTokens;
  }

  if (tokens != nullptr)
  {
    clang_disposeTokens(m_tu, tokens, numTokens);
  }

  return hr;
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetLocation(
  _In_ IDxcFile* file,
  unsigned line, unsigned column,
  IDxcSourceLocation** pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = nullptr;

  if (file == nullptr) return E_INVALIDARG;
  DxcThreadMalloc TM(m_pMalloc);
  DxcFile* fileImpl = reinterpret_cast<DxcFile*>(file);
  return DxcSourceLocation::Create(clang_getLocation(m_tu, fileImpl->GetFile(), line, column), pResult);
}

HRESULT DxcTranslationUnit::GetNumDiagnostics(_Out_ unsigned* pValue)
{
  if (pValue == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  *pValue = clang_getNumDiagnostics(m_tu);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetDiagnostic(unsigned index, IDxcDiagnostic** pValue)
{
  if (pValue == nullptr) return E_POINTER;
  DxcThreadMalloc TM(m_pMalloc);
  return DxcDiagnostic::Create(clang_getDiagnostic(m_tu, index), pValue);
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetFile(LPCSTR name, IDxcFile** pResult)
{
  if (name == nullptr) return E_INVALIDARG;
  if (pResult == nullptr) return E_POINTER;
  *pResult = nullptr;

  // TODO: until an interface to file access is defined and implemented, simply fall back to pure Win32/CRT calls.
  DxcThreadMalloc TM(m_pMalloc);
  ::llvm::sys::fs::MSFileSystem* msfPtr;
  IFR(CreateMSFileSystemForDisk(&msfPtr));
  std::auto_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
  ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());

  CXFile localFile = clang_getFile(m_tu, name);
  return localFile == nullptr ? DISP_E_BADINDEX : DxcFile::Create(localFile, pResult);
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetFileName(LPSTR* pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToAnsiAndDispose(clang_getTranslationUnitSpelling(m_tu), pResult);
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::Reparse(
  IDxcUnsavedFile** unsaved_files,
  unsigned num_unsaved_files)
{
  HRESULT hr;
  CXUnsavedFile* local_unsaved_files;
  DxcThreadMalloc TM(m_pMalloc);
  hr = SetupUnsavedFiles(unsaved_files, num_unsaved_files, &local_unsaved_files);
  if (FAILED(hr)) return hr;
  int reparseResult = clang_reparseTranslationUnit(
    m_tu, num_unsaved_files, local_unsaved_files, clang_defaultReparseOptions(m_tu));
  CleanupUnsavedFiles(local_unsaved_files, num_unsaved_files);
  return reparseResult == 0 ? S_OK : E_FAIL;
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetCursorForLocation(IDxcSourceLocation* location, IDxcCursor** pResult)
{
  if (location == nullptr) return E_INVALIDARG;
  if (pResult == nullptr) return E_POINTER;
  DxcSourceLocation* locationImpl = reinterpret_cast<DxcSourceLocation*>(location);
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCursor::Create(clang_getCursor(m_tu, locationImpl->GetLocation()), pResult);
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetLocationForOffset(IDxcFile* file, unsigned offset, IDxcSourceLocation** pResult)
{
  if (file == nullptr) return E_INVALIDARG;
  if (pResult == nullptr) return E_POINTER;
  DxcFile* fileImpl = reinterpret_cast<DxcFile*>(file);
  DxcThreadMalloc TM(m_pMalloc);
  return DxcSourceLocation::Create(clang_getLocationForOffset(m_tu, fileImpl->GetFile(), offset), pResult);
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetSkippedRanges(IDxcFile* file, unsigned* pResultCount, IDxcSourceRange*** pResult)
{
  if (file == nullptr) return E_INVALIDARG;
  if (pResultCount == nullptr) return E_POINTER;
  if (pResult == nullptr) return E_POINTER;

  *pResultCount = 0;
  *pResult = nullptr;

  DxcThreadMalloc TM(m_pMalloc);
  DxcFile* fileImpl = reinterpret_cast<DxcFile*>(file);

  unsigned len = clang_ms_countSkippedRanges(m_tu, fileImpl->GetFile());
  if (len == 0)
  {
    return S_OK;
  }

  CoTaskMemAllocZeroElems(len, pResult);
  if (*pResult == nullptr)
  {
    return E_OUTOFMEMORY;
  }

  HRESULT hr = S_OK;
  CXSourceRange* ranges = new CXSourceRange[len];
  clang_ms_getSkippedRanges(m_tu, fileImpl->GetFile(), ranges, len);
  for (unsigned i = 0; i < len; ++i)
  {
    hr = DxcSourceRange::Create(ranges[i], &(*pResult)[i]);
    if (FAILED(hr)) break;
  }

  // Cleanup partial progress.
  if (FAILED(hr))
  {
    SafeReleaseIfaceArray(*pResult, len);
    CoTaskMemFree(*pResult);
    *pResult = nullptr;
  }
  else
  {
    *pResultCount = len;
  }

  delete[] ranges;

  return hr;
}

HRESULT DxcTranslationUnit::GetDiagnosticDetails(
  unsigned index,
  DxcDiagnosticDisplayOptions options,
  _Out_ unsigned* errorCode,
  _Out_ unsigned* errorLine,
  _Out_ unsigned* errorColumn,
  _Out_ BSTR* errorFile,
  _Out_ unsigned* errorOffset,
  _Out_ unsigned* errorLength,
  _Out_ BSTR* errorMessage)
{
  if (errorCode == nullptr || errorLine == nullptr ||
      errorColumn == nullptr || errorFile == nullptr ||
      errorOffset == nullptr || errorLength == nullptr ||
      errorMessage == nullptr)
  {
    return E_POINTER;
  }

  *errorCode = *errorLine = *errorColumn = *errorOffset = *errorLength = 0;
  *errorFile = *errorMessage = nullptr;

  HRESULT hr = S_OK;
  DxcThreadMalloc TM(m_pMalloc);
  CXDiagnostic diag = clang_getDiagnostic(m_tu, index);
  hr = CXStringToBSTRAndDispose(clang_formatDiagnostic(diag, options), errorMessage);
  if (FAILED(hr))
  {
    return hr;
  }

  CXSourceLocation diagLoc = clang_getDiagnosticLocation(diag);
  CXFile diagFile;
  clang_getSpellingLocation(diagLoc, &diagFile, errorLine, errorColumn, errorOffset);
  hr = CXStringToBSTRAndDispose(clang_getFileName(diagFile), errorFile);
  if (FAILED(hr))
  {
    SysFreeString(*errorMessage);
    *errorMessage = nullptr;
    return hr;
  }

  return S_OK;
}

struct InclusionData {
  HRESULT result;
  CSimpleArray<CComPtr<IDxcInclusion>> inclusions;
};

static
void VisitInclusion(CXFile included_file,
  CXSourceLocation* inclusion_stack,
  unsigned include_len,
  CXClientData client_data) {
  InclusionData* D = (InclusionData *)client_data;
  if (SUCCEEDED(D->result)) {
    CComPtr<IDxcInclusion> pInclusion;
    HRESULT hr = DxcInclusion::Create(included_file, include_len, inclusion_stack, &pInclusion);
    if (FAILED(hr)) {
      D->result = E_FAIL;
    }
    else if (!D->inclusions.Add(pInclusion)) {
      D->result = E_OUTOFMEMORY;
    }
  }
}

_Use_decl_annotations_
HRESULT DxcTranslationUnit::GetInclusionList(unsigned *pResultCount,
                                             IDxcInclusion ***pResult) {
  if (pResultCount == nullptr || pResult == nullptr) {
    return E_POINTER;
  }

  *pResultCount = 0;
  *pResult = nullptr;
  DxcThreadMalloc TM(m_pMalloc);
  InclusionData D;
  D.result = S_OK;
  clang_getInclusions(m_tu, VisitInclusion, &D);
  if (FAILED(D.result)) {
    return D.result;
  }
  int inclusionCount = D.inclusions.GetSize();
  if (inclusionCount > 0) {
    CoTaskMemAllocZeroElems<IDxcInclusion *>(inclusionCount, pResult);
    if (*pResult == nullptr) {
      return E_OUTOFMEMORY;
    }
    for (int i = 0; i < inclusionCount; ++i) {
      (*pResult)[i] = D.inclusions[i].Detach();
    }
    *pResultCount = inclusionCount;
  }
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

DxcType::DxcType()
{
  m_pMalloc = DxcGetThreadMallocNoRef();
}

DxcType::~DxcType()
{
}

_Use_decl_annotations_
HRESULT DxcType::Create(const CXType& type, IDxcType** pObject)
{
  if (pObject == nullptr) return E_POINTER;
  *pObject = nullptr;
  DxcType* newValue = new (std::nothrow) DxcType();
  if (newValue == nullptr) return E_OUTOFMEMORY;
  newValue->Initialize(type);
  newValue->AddRef();
  *pObject = newValue;
  return S_OK;
}

void DxcType::Initialize(const CXType& type)
{
  m_type = type;
}

_Use_decl_annotations_
HRESULT DxcType::GetSpelling(LPSTR* pResult)
{
  DxcThreadMalloc TM(m_pMalloc);
  return CXStringToAnsiAndDispose(clang_getTypeSpelling(m_type), pResult);
}

_Use_decl_annotations_
HRESULT DxcType::IsEqualTo(IDxcType* other, BOOL* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  if (other == nullptr)
  {
    *pResult = FALSE;
    return S_OK;
  }
  DxcType* otherImpl = reinterpret_cast<DxcType*>(other);
  *pResult = 0 != clang_equalTypes(m_type, otherImpl->m_type);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcType::GetKind(DxcTypeKind* pResult)
{
  if (pResult == nullptr) return E_POINTER;
  *pResult = (DxcTypeKind)m_type.kind;
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

C_ASSERT(DxcCursor_UnexposedDecl == CXCursor_UnexposedDecl);
C_ASSERT(DxcCursor_StructDecl == CXCursor_StructDecl);
C_ASSERT(DxcCursor_UnionDecl == CXCursor_UnionDecl);
C_ASSERT(DxcCursor_ClassDecl == CXCursor_ClassDecl);
C_ASSERT(DxcCursor_EnumDecl == CXCursor_EnumDecl);
C_ASSERT(DxcCursor_FieldDecl == CXCursor_FieldDecl);
C_ASSERT(DxcCursor_EnumConstantDecl == CXCursor_EnumConstantDecl);
C_ASSERT(DxcCursor_FunctionDecl == CXCursor_FunctionDecl);
C_ASSERT(DxcCursor_VarDecl == CXCursor_VarDecl);
C_ASSERT(DxcCursor_ParmDecl == CXCursor_ParmDecl);
C_ASSERT(DxcCursor_ObjCInterfaceDecl == CXCursor_ObjCInterfaceDecl);
C_ASSERT(DxcCursor_ObjCCategoryDecl == CXCursor_ObjCCategoryDecl);
C_ASSERT(DxcCursor_ObjCProtocolDecl == CXCursor_ObjCProtocolDecl);
C_ASSERT(DxcCursor_ObjCPropertyDecl == CXCursor_ObjCPropertyDecl);
C_ASSERT(DxcCursor_ObjCIvarDecl == CXCursor_ObjCIvarDecl);
C_ASSERT(DxcCursor_ObjCInstanceMethodDecl == CXCursor_ObjCInstanceMethodDecl);
C_ASSERT(DxcCursor_ObjCClassMethodDecl == CXCursor_ObjCClassMethodDecl);
C_ASSERT(DxcCursor_ObjCImplementationDecl == CXCursor_ObjCImplementationDecl);
C_ASSERT(DxcCursor_ObjCCategoryImplDecl == CXCursor_ObjCCategoryImplDecl);
C_ASSERT(DxcCursor_TypedefDecl == CXCursor_TypedefDecl);
C_ASSERT(DxcCursor_CXXMethod == CXCursor_CXXMethod);
C_ASSERT(DxcCursor_Namespace == CXCursor_Namespace);
C_ASSERT(DxcCursor_LinkageSpec == CXCursor_LinkageSpec);
C_ASSERT(DxcCursor_Constructor == CXCursor_Constructor);
C_ASSERT(DxcCursor_Destructor == CXCursor_Destructor);
C_ASSERT(DxcCursor_ConversionFunction == CXCursor_ConversionFunction);
C_ASSERT(DxcCursor_TemplateTypeParameter == CXCursor_TemplateTypeParameter);
C_ASSERT(DxcCursor_NonTypeTemplateParameter == CXCursor_NonTypeTemplateParameter);
C_ASSERT(DxcCursor_TemplateTemplateParameter == CXCursor_TemplateTemplateParameter);
C_ASSERT(DxcCursor_FunctionTemplate == CXCursor_FunctionTemplate);
C_ASSERT(DxcCursor_ClassTemplate == CXCursor_ClassTemplate);
C_ASSERT(DxcCursor_ClassTemplatePartialSpecialization == CXCursor_ClassTemplatePartialSpecialization);
C_ASSERT(DxcCursor_NamespaceAlias == CXCursor_NamespaceAlias);
C_ASSERT(DxcCursor_UsingDirective == CXCursor_UsingDirective);
C_ASSERT(DxcCursor_UsingDeclaration == CXCursor_UsingDeclaration);
C_ASSERT(DxcCursor_TypeAliasDecl == CXCursor_TypeAliasDecl);
C_ASSERT(DxcCursor_ObjCSynthesizeDecl == CXCursor_ObjCSynthesizeDecl);
C_ASSERT(DxcCursor_ObjCDynamicDecl == CXCursor_ObjCDynamicDecl);
C_ASSERT(DxcCursor_CXXAccessSpecifier == CXCursor_CXXAccessSpecifier);
C_ASSERT(DxcCursor_FirstDecl == CXCursor_FirstDecl);
C_ASSERT(DxcCursor_LastDecl == CXCursor_LastDecl);
C_ASSERT(DxcCursor_FirstRef == CXCursor_FirstRef);
C_ASSERT(DxcCursor_ObjCSuperClassRef == CXCursor_ObjCSuperClassRef);
C_ASSERT(DxcCursor_ObjCProtocolRef == CXCursor_ObjCProtocolRef);
C_ASSERT(DxcCursor_ObjCClassRef == CXCursor_ObjCClassRef);
C_ASSERT(DxcCursor_TypeRef == CXCursor_TypeRef);
C_ASSERT(DxcCursor_CXXBaseSpecifier == CXCursor_CXXBaseSpecifier);
C_ASSERT(DxcCursor_TemplateRef == CXCursor_TemplateRef);
C_ASSERT(DxcCursor_NamespaceRef == CXCursor_NamespaceRef);
C_ASSERT(DxcCursor_MemberRef == CXCursor_MemberRef);
C_ASSERT(DxcCursor_LabelRef == CXCursor_LabelRef);
C_ASSERT(DxcCursor_OverloadedDeclRef == CXCursor_OverloadedDeclRef);
C_ASSERT(DxcCursor_VariableRef == CXCursor_VariableRef);
C_ASSERT(DxcCursor_LastRef == CXCursor_LastRef);
C_ASSERT(DxcCursor_FirstInvalid == CXCursor_FirstInvalid);
C_ASSERT(DxcCursor_InvalidFile == CXCursor_InvalidFile);
C_ASSERT(DxcCursor_NoDeclFound == CXCursor_NoDeclFound);
C_ASSERT(DxcCursor_NotImplemented == CXCursor_NotImplemented);
C_ASSERT(DxcCursor_InvalidCode == CXCursor_InvalidCode);
C_ASSERT(DxcCursor_LastInvalid == CXCursor_LastInvalid);
C_ASSERT(DxcCursor_FirstExpr == CXCursor_FirstExpr);
C_ASSERT(DxcCursor_UnexposedExpr == CXCursor_UnexposedExpr);
C_ASSERT(DxcCursor_DeclRefExpr == CXCursor_DeclRefExpr);
C_ASSERT(DxcCursor_MemberRefExpr == CXCursor_MemberRefExpr);
C_ASSERT(DxcCursor_CallExpr == CXCursor_CallExpr);
C_ASSERT(DxcCursor_ObjCMessageExpr == CXCursor_ObjCMessageExpr);
C_ASSERT(DxcCursor_BlockExpr == CXCursor_BlockExpr);
C_ASSERT(DxcCursor_IntegerLiteral == CXCursor_IntegerLiteral);
C_ASSERT(DxcCursor_FloatingLiteral == CXCursor_FloatingLiteral);
C_ASSERT(DxcCursor_ImaginaryLiteral == CXCursor_ImaginaryLiteral);
C_ASSERT(DxcCursor_StringLiteral == CXCursor_StringLiteral);
C_ASSERT(DxcCursor_CharacterLiteral == CXCursor_CharacterLiteral);
C_ASSERT(DxcCursor_ParenExpr == CXCursor_ParenExpr);
C_ASSERT(DxcCursor_UnaryOperator == CXCursor_UnaryOperator);
C_ASSERT(DxcCursor_ArraySubscriptExpr == CXCursor_ArraySubscriptExpr);
C_ASSERT(DxcCursor_BinaryOperator == CXCursor_BinaryOperator);
C_ASSERT(DxcCursor_CompoundAssignOperator == CXCursor_CompoundAssignOperator);
C_ASSERT(DxcCursor_ConditionalOperator == CXCursor_ConditionalOperator);
C_ASSERT(DxcCursor_CStyleCastExpr == CXCursor_CStyleCastExpr);
C_ASSERT(DxcCursor_CompoundLiteralExpr == CXCursor_CompoundLiteralExpr);
C_ASSERT(DxcCursor_InitListExpr == CXCursor_InitListExpr);
C_ASSERT(DxcCursor_AddrLabelExpr == CXCursor_AddrLabelExpr);
C_ASSERT(DxcCursor_StmtExpr == CXCursor_StmtExpr);
C_ASSERT(DxcCursor_GenericSelectionExpr == CXCursor_GenericSelectionExpr);
C_ASSERT(DxcCursor_GNUNullExpr == CXCursor_GNUNullExpr);
C_ASSERT(DxcCursor_CXXStaticCastExpr == CXCursor_CXXStaticCastExpr);
C_ASSERT(DxcCursor_CXXDynamicCastExpr == CXCursor_CXXDynamicCastExpr);
C_ASSERT(DxcCursor_CXXReinterpretCastExpr == CXCursor_CXXReinterpretCastExpr);
C_ASSERT(DxcCursor_CXXConstCastExpr == CXCursor_CXXConstCastExpr);
C_ASSERT(DxcCursor_CXXFunctionalCastExpr == CXCursor_CXXFunctionalCastExpr);
C_ASSERT(DxcCursor_CXXTypeidExpr == CXCursor_CXXTypeidExpr);
C_ASSERT(DxcCursor_CXXBoolLiteralExpr == CXCursor_CXXBoolLiteralExpr);
C_ASSERT(DxcCursor_CXXNullPtrLiteralExpr == CXCursor_CXXNullPtrLiteralExpr);
C_ASSERT(DxcCursor_CXXThisExpr == CXCursor_CXXThisExpr);
C_ASSERT(DxcCursor_CXXThrowExpr == CXCursor_CXXThrowExpr);
C_ASSERT(DxcCursor_CXXNewExpr == CXCursor_CXXNewExpr);
C_ASSERT(DxcCursor_CXXDeleteExpr == CXCursor_CXXDeleteExpr);
C_ASSERT(DxcCursor_UnaryExpr == CXCursor_UnaryExpr);
C_ASSERT(DxcCursor_ObjCStringLiteral == CXCursor_ObjCStringLiteral);
C_ASSERT(DxcCursor_ObjCEncodeExpr == CXCursor_ObjCEncodeExpr);
C_ASSERT(DxcCursor_ObjCSelectorExpr == CXCursor_ObjCSelectorExpr);
C_ASSERT(DxcCursor_ObjCProtocolExpr == CXCursor_ObjCProtocolExpr);
C_ASSERT(DxcCursor_ObjCBridgedCastExpr == CXCursor_ObjCBridgedCastExpr);
C_ASSERT(DxcCursor_PackExpansionExpr == CXCursor_PackExpansionExpr);
C_ASSERT(DxcCursor_SizeOfPackExpr == CXCursor_SizeOfPackExpr);
C_ASSERT(DxcCursor_LambdaExpr == CXCursor_LambdaExpr);
C_ASSERT(DxcCursor_ObjCBoolLiteralExpr == CXCursor_ObjCBoolLiteralExpr);
C_ASSERT(DxcCursor_ObjCSelfExpr == CXCursor_ObjCSelfExpr);
C_ASSERT(DxcCursor_LastExpr == CXCursor_LastExpr);
C_ASSERT(DxcCursor_FirstStmt == CXCursor_FirstStmt);
C_ASSERT(DxcCursor_UnexposedStmt == CXCursor_UnexposedStmt);
C_ASSERT(DxcCursor_LabelStmt == CXCursor_LabelStmt);
C_ASSERT(DxcCursor_CompoundStmt == CXCursor_CompoundStmt);
C_ASSERT(DxcCursor_CaseStmt == CXCursor_CaseStmt);
C_ASSERT(DxcCursor_DefaultStmt == CXCursor_DefaultStmt);
C_ASSERT(DxcCursor_IfStmt == CXCursor_IfStmt);
C_ASSERT(DxcCursor_SwitchStmt == CXCursor_SwitchStmt);
C_ASSERT(DxcCursor_WhileStmt == CXCursor_WhileStmt);
C_ASSERT(DxcCursor_DoStmt == CXCursor_DoStmt);
C_ASSERT(DxcCursor_ForStmt == CXCursor_ForStmt);
C_ASSERT(DxcCursor_GotoStmt == CXCursor_GotoStmt);
C_ASSERT(DxcCursor_IndirectGotoStmt == CXCursor_IndirectGotoStmt);
C_ASSERT(DxcCursor_ContinueStmt == CXCursor_ContinueStmt);
C_ASSERT(DxcCursor_BreakStmt == CXCursor_BreakStmt);
C_ASSERT(DxcCursor_ReturnStmt == CXCursor_ReturnStmt);
C_ASSERT(DxcCursor_GCCAsmStmt == CXCursor_GCCAsmStmt);
C_ASSERT(DxcCursor_AsmStmt == CXCursor_AsmStmt);
C_ASSERT(DxcCursor_ObjCAtTryStmt == CXCursor_ObjCAtTryStmt);
C_ASSERT(DxcCursor_ObjCAtCatchStmt == CXCursor_ObjCAtCatchStmt);
C_ASSERT(DxcCursor_ObjCAtFinallyStmt == CXCursor_ObjCAtFinallyStmt);
C_ASSERT(DxcCursor_ObjCAtThrowStmt == CXCursor_ObjCAtThrowStmt);
C_ASSERT(DxcCursor_ObjCAtSynchronizedStmt == CXCursor_ObjCAtSynchronizedStmt);
C_ASSERT(DxcCursor_ObjCAutoreleasePoolStmt == CXCursor_ObjCAutoreleasePoolStmt);
C_ASSERT(DxcCursor_ObjCForCollectionStmt == CXCursor_ObjCForCollectionStmt);
C_ASSERT(DxcCursor_CXXCatchStmt == CXCursor_CXXCatchStmt);
C_ASSERT(DxcCursor_CXXTryStmt == CXCursor_CXXTryStmt);
C_ASSERT(DxcCursor_CXXForRangeStmt == CXCursor_CXXForRangeStmt);
C_ASSERT(DxcCursor_SEHTryStmt == CXCursor_SEHTryStmt);
C_ASSERT(DxcCursor_SEHExceptStmt == CXCursor_SEHExceptStmt);
C_ASSERT(DxcCursor_SEHFinallyStmt == CXCursor_SEHFinallyStmt);
C_ASSERT(DxcCursor_MSAsmStmt == CXCursor_MSAsmStmt);
C_ASSERT(DxcCursor_NullStmt == CXCursor_NullStmt);
C_ASSERT(DxcCursor_DeclStmt == CXCursor_DeclStmt);
C_ASSERT(DxcCursor_OMPParallelDirective == CXCursor_OMPParallelDirective);
C_ASSERT(DxcCursor_LastStmt == CXCursor_LastStmt);
C_ASSERT(DxcCursor_TranslationUnit == CXCursor_TranslationUnit);
C_ASSERT(DxcCursor_FirstAttr == CXCursor_FirstAttr);
C_ASSERT(DxcCursor_UnexposedAttr == CXCursor_UnexposedAttr);
C_ASSERT(DxcCursor_IBActionAttr == CXCursor_IBActionAttr);
C_ASSERT(DxcCursor_IBOutletAttr == CXCursor_IBOutletAttr);
C_ASSERT(DxcCursor_IBOutletCollectionAttr == CXCursor_IBOutletCollectionAttr);
C_ASSERT(DxcCursor_CXXFinalAttr == CXCursor_CXXFinalAttr);
C_ASSERT(DxcCursor_CXXOverrideAttr == CXCursor_CXXOverrideAttr);
C_ASSERT(DxcCursor_AnnotateAttr == CXCursor_AnnotateAttr);
C_ASSERT(DxcCursor_AsmLabelAttr == CXCursor_AsmLabelAttr);
C_ASSERT(DxcCursor_PackedAttr == CXCursor_PackedAttr);
C_ASSERT(DxcCursor_LastAttr == CXCursor_LastAttr);
C_ASSERT(DxcCursor_PreprocessingDirective == CXCursor_PreprocessingDirective);
C_ASSERT(DxcCursor_MacroDefinition == CXCursor_MacroDefinition);
C_ASSERT(DxcCursor_MacroExpansion == CXCursor_MacroExpansion);
C_ASSERT(DxcCursor_MacroInstantiation == CXCursor_MacroInstantiation);
C_ASSERT(DxcCursor_InclusionDirective == CXCursor_InclusionDirective);
C_ASSERT(DxcCursor_FirstPreprocessing == CXCursor_FirstPreprocessing);
C_ASSERT(DxcCursor_LastPreprocessing == CXCursor_LastPreprocessing);
C_ASSERT(DxcCursor_ModuleImportDecl == CXCursor_ModuleImportDecl);
C_ASSERT(DxcCursor_FirstExtraDecl == CXCursor_FirstExtraDecl);
C_ASSERT(DxcCursor_LastExtraDecl == CXCursor_LastExtraDecl);

C_ASSERT(DxcTranslationUnitFlags_UseCallerThread == CXTranslationUnit_UseCallerThread);
