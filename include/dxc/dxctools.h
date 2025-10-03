///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxctools.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides declarations for the DirectX Compiler tooling components.        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXC_TOOLS__
#define __DXC_TOOLS__

#include <dxc/dxcapi.h>
#include "d3d12shader.h"

enum RewriterOptionMask {
  Default = 0,
  SkipFunctionBody = 1,
  SkipStatic = 2,
  GlobalExternByDefault = 4,
  KeepUserMacro = 8,
};

CROSS_PLATFORM_UUIDOF(IDxcRewriter, "c012115b-8893-4eb9-9c5a-111456ea1c45")
struct IDxcRewriter : public IUnknown {

  virtual HRESULT STDMETHODCALLTYPE RemoveUnusedGlobals(
      IDxcBlobEncoding *pSource, LPCWSTR entryPoint, DxcDefine *pDefines,
      UINT32 defineCount, IDxcOperationResult **ppResult) = 0;

  virtual HRESULT STDMETHODCALLTYPE
  RewriteUnchanged(IDxcBlobEncoding *pSource, DxcDefine *pDefines,
                   UINT32 defineCount, IDxcOperationResult **ppResult) = 0;

  virtual HRESULT STDMETHODCALLTYPE RewriteUnchangedWithInclude(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName, DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler, UINT32 rewriteOption,
      IDxcOperationResult **ppResult) = 0;
};

#ifdef _MSC_VER
#define CLSID_SCOPE __declspec(selectany) extern
#else
#define CLSID_SCOPE
#endif

CLSID_SCOPE const CLSID
    CLSID_DxcRewriter = {/* b489b951-e07f-40b3-968d-93e124734da4 */
                         0xb489b951,
                         0xe07f,
                         0x40b3,
                         {0x96, 0x8d, 0x93, 0xe1, 0x24, 0x73, 0x4d, 0xa4}};

CROSS_PLATFORM_UUIDOF(IDxcRewriter2, "261afca1-0609-4ec6-a77f-d98c7035194e")
struct IDxcRewriter2 : public IDxcRewriter {

  virtual HRESULT STDMETHODCALLTYPE RewriteWithOptions(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName,
      // Compiler arguments
      LPCWSTR *pArguments, UINT32 argCount,
      // Defines
      DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler, IDxcOperationResult **ppResult) = 0;
};

//Expose HLSL reflection before DXIL/SPIRV generation.
//(Ran after the preprocessed HLSL is obtained).
//This is useful to avoid custom parsers from reinventing the wheel.
//You could use it to find all entrypoints even if [shader("")] isn't used,
//Find struct/enum information, find out about optimized out registers, etc.

enum D3D12_HLSL_REFLECTION_FEATURE {

  D3D12_HLSL_REFLECTION_FEATURE_NONE = 0,

  // Includes cbuffer and registers only
  D3D12_HLSL_REFLECTION_FEATURE_BASICS = 1 << 0,

  D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS = 1 << 1,
  D3D12_HLSL_REFLECTION_FEATURE_NAMESPACES = 1 << 2,

  // Include user types (struct, enum, typedef, etc.)
  D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES = 1 << 3,

  // Variables, structs, functions defined in functions
  D3D12_HLSL_REFLECTION_FEATURE_SCOPES = 1 << 4,

  // Symbol info (stripping this will remove names and file location info)
  D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO = 1 << 16,

  D3D12_HLSL_REFLECTION_FEATURE_ALL = D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO - 1
};

inline D3D12_HLSL_REFLECTION_FEATURE &operator|=(D3D12_HLSL_REFLECTION_FEATURE &a,
                                          D3D12_HLSL_REFLECTION_FEATURE b) {
  return a = (D3D12_HLSL_REFLECTION_FEATURE)(uint32_t(a) | uint32_t(b));
}

inline D3D12_HLSL_REFLECTION_FEATURE &
operator&=(D3D12_HLSL_REFLECTION_FEATURE &a,
                                          D3D12_HLSL_REFLECTION_FEATURE b) {
  return a = (D3D12_HLSL_REFLECTION_FEATURE)(uint32_t(a) & uint32_t(b));
}

inline D3D12_HLSL_REFLECTION_FEATURE
operator~(D3D12_HLSL_REFLECTION_FEATURE a) {
  return (D3D12_HLSL_REFLECTION_FEATURE) ~uint32_t(a);
}

struct D3D12_HLSL_REFLECTION_DESC {
  D3D12_HLSL_REFLECTION_FEATURE Features;
  UINT ConstantBufferCount;
  UINT ResourceCount;
  UINT FunctionCount;
  UINT EnumCount;
  UINT NodeCount;
  UINT TypeCount;
  UINT StructCount;
  UINT UnionCount;
  UINT InterfaceCount;
};

struct D3D12_HLSL_FUNCTION_DESC {
  LPCSTR Name;                  // Function name
  UINT FunctionParameterCount;  // Number of logical parameters in the function
                                // signature (not including return)
  BOOL HasReturn;               // TRUE, if function returns a value, false - it is a
                                // subroutine
};

enum D3D12_HLSL_ENUM_TYPE {
  
  D3D12_HLSL_ENUM_TYPE_UINT,
  D3D12_HLSL_ENUM_TYPE_INT,
  D3D12_HLSL_ENUM_TYPE_UINT64_T,
  D3D12_HLSL_ENUM_TYPE_INT64_T,
  D3D12_HLSL_ENUM_TYPE_UINT16_T,
  D3D12_HLSL_ENUM_TYPE_INT16_T,

  D3D12_HLSL_ENUM_TYPE_START = D3D12_HLSL_ENUM_TYPE_UINT,
  D3D12_HLSL_ENUM_TYPE_END = D3D12_HLSL_ENUM_TYPE_INT16_T
};

struct D3D12_HLSL_ENUM_DESC {
  LPCSTR Name;
  UINT ValueCount;
  D3D12_HLSL_ENUM_TYPE Type;
};

struct D3D12_HLSL_ENUM_VALUE {
  LPCSTR Name;
  INT64 Value;
};

struct D3D12_HLSL_ANNOTATION {
  LPCSTR Name;
  BOOL IsBuiltin;
};

enum D3D12_HLSL_NODE_TYPE {

  D3D12_HLSL_NODE_TYPE_REGISTER,
  D3D12_HLSL_NODE_TYPE_FUNCTION,
  D3D12_HLSL_NODE_TYPE_ENUM,
  D3D12_HLSL_NODE_TYPE_ENUM_VALUE,
  D3D12_HLSL_NODE_TYPE_NAMESPACE,

  D3D12_HLSL_NODE_TYPE_VARIABLE, // localId points to the type
  D3D12_HLSL_NODE_TYPE_TYPEDEF,  // ^

  D3D12_HLSL_NODE_TYPE_STRUCT,   // has Variables as member like buffers do,
                                 // localId is the typeId (if not fwd decl)
  D3D12_HLSL_NODE_TYPE_UNION,    // ^

  D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE,

  D3D12_HLSL_NODE_TYPE_INTERFACE,
  D3D12_HLSL_NODE_TYPE_PARAMETER,

  // TODO: D3D12_HLSL_NODE_TYPE_USING,

  D3D12_HLSL_NODE_TYPE_RESERVED =
      1 << 7, // Highest bit; reserved as an indicator for fwd declarations

  D3D12_HLSL_NODE_TYPE_START = D3D12_HLSL_NODE_TYPE_REGISTER,
  D3D12_HLSL_NODE_TYPE_END = D3D12_HLSL_NODE_TYPE_PARAMETER
};

struct D3D12_HLSL_NODE {
  LPCSTR Name;
  LPCSTR Semantic;
  D3D12_HLSL_NODE_TYPE Type;
  UINT LocalId;
  UINT ChildCount;
  UINT Parent;
  UINT AnnotationCount;
  UINT FwdBckDeclareNode;           //If UINT_MAX has no forward / backward declare
  BOOL IsFwdDeclare;
};

struct D3D12_HLSL_NODE_SYMBOL {
  LPCSTR FileName;
  UINT LineId;
  UINT LineCount;
  UINT ColumnStart;
  UINT ColumnEnd;
};

// TODO: Move to d3d12shader.h

struct D3D12_ARRAY_DESC {
  uint32_t ArrayDims;
  uint32_t ArrayLengths[32]; // SPV_REFLECT_MAX_ARRAY_DIMS
};

struct D3D12_SHADER_INPUT_BIND_DESC1 {
  D3D12_SHADER_INPUT_BIND_DESC Desc;
  D3D12_ARRAY_DESC ArrayInfo;
};

DECLARE_INTERFACE_(ID3D12ShaderReflectionType1, ID3D12ShaderReflectionType) {
  STDMETHOD(GetArrayDesc)(THIS_ _Out_ D3D12_ARRAY_DESC * pArrayDesc) PURE;
};

typedef interface IDxcHLSLReflection IDxcHLSLReflection;

// {7016F834-AE85-4C86-A473-8C2C981DD370}
interface DECLSPEC_UUID("7016f834-ae85-4c86-a473-8c2c981dd370")
    IDxcHLSLReflection;
DEFINE_GUID(IID_IDxcHLSLReflection, 0x7016f834, 0xae85, 0x4c86, 0xa4, 0x73, 0x8c,
            0x2c, 0x98, 0x1d, 0xd3, 0x70);

#undef INTERFACE
#define INTERFACE IDxcHLSLReflection

DECLARE_INTERFACE(IDxcHLSLReflection) {

  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_HLSL_REFLECTION_DESC *pDesc) PURE;

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByIndex)
  (THIS_ _In_ UINT Index) PURE;

  // The D3D12_SHADER_INPUT_BIND_DESC permits providing invalid Space and
  // BindPoint. In the future, implementations could decide to return this
  // depending on the backend. But since this is HLSL frontend reflection, we don't
  // know the bindings on the backend.

  STDMETHOD(GetResourceBindingDesc)
  (THIS_ _In_ UINT ResourceIndex, _Out_ D3D12_SHADER_INPUT_BIND_DESC1 *pDesc)
      PURE;

  STDMETHOD(GetFunctionDesc)
  (THIS_ _In_ UINT FunctionIndex, THIS_ _Out_ D3D12_HLSL_FUNCTION_DESC *pDesc)
      PURE;

  // Use D3D_RETURN_PARAMETER_INDEX to get description of the return value.
  STDMETHOD_(ID3D12FunctionParameterReflection *, GetFunctionParameter)
  (THIS_ _In_ UINT FunctionIndex, THIS_ _In_ INT ParameterIndex) PURE;

  STDMETHOD(GetStructTypeByIndex)
  (THIS_ _In_ UINT Index, _Outptr_ ID3D12ShaderReflectionType **ppType)
      PURE;

  STDMETHOD(GetUnionTypeByIndex)
  (THIS_ _In_ UINT Index, _Outptr_ ID3D12ShaderReflectionType **ppType)
      PURE;

  STDMETHOD(GetInterfaceTypeByIndex)
  (THIS_ _In_ UINT Index, _Outptr_ ID3D12ShaderReflectionType **ppType)
      PURE;

  STDMETHOD(GetTypeByIndex)
  (THIS_ _In_ UINT Index, _Outptr_ ID3D12ShaderReflectionType **ppType)
      PURE;

  STDMETHOD(GetEnumDesc)
  (THIS_ _In_ UINT EnumIndex, _Out_ D3D12_HLSL_ENUM_DESC *pDesc) PURE;

  STDMETHOD(GetEnumValueByIndex)
  (THIS_ _In_ UINT EnumIndex, _In_ UINT ValueIndex,
   _Out_ D3D12_HLSL_ENUM_VALUE *pValueDesc) PURE;

  STDMETHOD(GetAnnotationByIndex)
  (THIS_ _In_ UINT NodeId, _In_ UINT Index,
   _Out_ D3D12_HLSL_ANNOTATION *pAnnotation) PURE;

  STDMETHOD(GetNodeDesc)
  (THIS_ _In_ UINT NodeId, _Out_ D3D12_HLSL_NODE *pDesc) PURE;

  STDMETHOD(GetChildNode)
  (THIS_ _In_ UINT NodeId, THIS_ _In_ UINT ChildId,
   _Out_ UINT *pChildNodeId) PURE;

  STDMETHOD(GetChildDesc)
  (THIS_ _In_ UINT NodeId, THIS_ _In_ UINT ChildId,
   _Out_ D3D12_HLSL_NODE *pDesc) PURE;

  // Only available if symbols aren't stripped

  STDMETHOD(GetNodeSymbolDesc)
  (THIS_ _In_ UINT NodeId, _Out_ D3D12_HLSL_NODE_SYMBOL *pSymbol) PURE;

  // Name helpers

  STDMETHOD(GetNodeByName)
  (THIS_ _In_ LPCSTR Name, _Out_ UINT *pNodeId) PURE;

  STDMETHOD(GetNodeSymbolDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_NODE_SYMBOL *pSymbol) PURE;

  STDMETHOD(GetNodeDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_NODE *pDesc) PURE;

  STDMETHOD(GetAnnotationByIndexAndName)
  (THIS_ _In_ LPCSTR Name, _In_ UINT Index,
   _Out_ D3D12_HLSL_ANNOTATION *pAnnotation) PURE;

  STDMETHOD(GetEnumDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_ENUM_DESC *pDesc) PURE;

  STDMETHOD(GetEnumValueByNameAndIndex)
  (THIS_ _In_ LPCSTR Name, _In_ UINT ValueIndex,
   _Out_ D3D12_HLSL_ENUM_VALUE *pValueDesc) PURE;

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByName)
  (THIS_ _In_ LPCSTR Name) PURE;

  STDMETHOD(GetFunctionDescByName)
  (THIS_ _In_ LPCSTR Name, THIS_ _Out_ D3D12_HLSL_FUNCTION_DESC *pDesc) PURE;

  STDMETHOD(GetResourceBindingDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_SHADER_INPUT_BIND_DESC1 * pDesc) PURE;

  STDMETHOD(GetStructTypeByName)
  (THIS_ _In_ LPCSTR Name, _Outptr_ ID3D12ShaderReflectionType **ppType) PURE;

  STDMETHOD(GetUnionTypeByName)
  (THIS_ _In_ LPCSTR Name, _Outptr_ ID3D12ShaderReflectionType **ppType) PURE;

  STDMETHOD(GetInterfaceTypeByName)
  (THIS_ _In_ LPCSTR Name, _Outptr_ ID3D12ShaderReflectionType **ppType) PURE;
};

#undef INTERFACE

CLSID_SCOPE const CLSID
    CLSID_DxcReflector = {/* ba5a8d8e-bf71-435a-977f-1677d7bcccc1 */
                          0xba5a8d8e,
                          0xbf71,
                          0x435a,
                          {0x16, 0x77, 0xd7, 0xbc, 0xcc, 0xc1}};

CROSS_PLATFORM_UUIDOF(IDxcHLSLReflector, "ba5a8d8e-bf71-435a-977f-1677d7bcccc1")
struct IDxcHLSLReflector : public IUnknown {

  virtual HRESULT STDMETHODCALLTYPE FromSource(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName,
      // Compiler arguments
      LPCWSTR *pArguments, UINT32 argCount,
      // Defines
      DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler,
      IDxcOperationResult **ppResult) = 0;

  virtual HRESULT STDMETHODCALLTYPE
  FromBlob(IDxcBlob *data, IDxcHLSLReflection **ppReflection) = 0;

  virtual HRESULT STDMETHODCALLTYPE ToBlob(IDxcHLSLReflection *reflection,
                                           IDxcBlob **ppResult) = 0;
};

#endif
