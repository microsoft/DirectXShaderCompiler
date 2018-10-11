///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerReflection.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for reading DXIL container structures.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstIterator.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/HLSL/DxilFunctionProps.h"

#include <unordered_set>
#include "llvm/ADT/SetVector.h"

#include "dxc/dxcapi.h"

#ifdef LLVM_ON_WIN32
#include "d3d12shader.h" // for compatibility
#include "d3d11shader.h" // for compatibility

const GUID IID_ID3D11ShaderReflection_43 = {
    0x0a233719,
    0x3960,
    0x4578,
    {0x9d, 0x7c, 0x20, 0x3b, 0x8b, 0x1d, 0x9c, 0xc1}};
const GUID IID_ID3D11ShaderReflection_47 = {
    0x8d536ca1,
    0x0cca,
    0x4956,
    {0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84}};

using namespace llvm;
using namespace hlsl;

class DxilContainerReflection : public IDxcContainerReflection {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<IDxcBlob> m_container;
  const DxilContainerHeader *m_pHeader = nullptr;
  uint32_t m_headerLen = 0;
  bool IsLoaded() const { return m_pHeader != nullptr; }
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxilContainerReflection)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcContainerReflection>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pContainer) override;
  HRESULT STDMETHODCALLTYPE GetPartCount(_Out_ UINT32 *pResult) override;
  HRESULT STDMETHODCALLTYPE GetPartKind(UINT32 idx, _Out_ UINT32 *pResult) override;
  HRESULT STDMETHODCALLTYPE GetPartContent(UINT32 idx, _COM_Outptr_ IDxcBlob **ppResult) override;
  HRESULT STDMETHODCALLTYPE FindFirstPartKind(UINT32 kind, _Out_ UINT32 *pResult) override;
  HRESULT STDMETHODCALLTYPE GetPartReflection(UINT32 idx, REFIID iid, _COM_Outptr_ void **ppvObject) override;
};

class CShaderReflectionConstantBuffer;
class CShaderReflectionType;

enum class PublicAPI { D3D12 = 0, D3D11_47 = 1, D3D11_43 = 2 };

class DxilModuleReflection {
public:
  CComPtr<IDxcBlob> m_pContainer;
  LLVMContext Context;
  std::unique_ptr<Module> m_pModule; // Must come after LLVMContext, otherwise unique_ptr will over-delete.
  DxilModule *m_pDxilModule = nullptr;
  std::vector<std::unique_ptr<CShaderReflectionConstantBuffer>>    m_CBs;
  std::vector<D3D12_SHADER_INPUT_BIND_DESC>       m_Resources;
  std::vector<std::unique_ptr<CShaderReflectionType>> m_Types;
  void CreateReflectionObjects();
  void CreateReflectionObjectForResource(DxilResourceBase *R);

  HRESULT LoadModule(IDxcBlob *pBlob, const DxilPartHeader *pPart);

  // Common code
  ID3D12ShaderReflectionConstantBuffer* _GetConstantBufferByIndex(UINT Index);
  ID3D12ShaderReflectionConstantBuffer* _GetConstantBufferByName(LPCSTR Name);

  HRESULT _GetResourceBindingDesc(UINT ResourceIndex,
                                  _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc,
                                  PublicAPI api = PublicAPI::D3D12);

  ID3D12ShaderReflectionVariable* _GetVariableByName(LPCSTR Name);

  HRESULT _GetResourceBindingDescByName(LPCSTR Name,
                                        D3D12_SHADER_INPUT_BIND_DESC *pDesc,
                                        PublicAPI api = PublicAPI::D3D12);
};

class DxilShaderReflection : public DxilModuleReflection, public ID3D12ShaderReflection {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC>     m_InputSignature;
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC>     m_OutputSignature;
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC>     m_PatchConstantSignature;
  std::vector<std::unique_ptr<char[]>>            m_UpperCaseNames;
  void SetCBufferUsage();
  void CreateReflectionObjectsForSignature(
      const DxilSignature &Sig,
      std::vector<D3D12_SIGNATURE_PARAMETER_DESC> &Descs);
  LPCSTR CreateUpperCase(LPCSTR pValue);
  void MarkUsedSignatureElements();
public:
  PublicAPI m_PublicAPI;
  void SetPublicAPI(PublicAPI value) { m_PublicAPI = value; }
  static PublicAPI IIDToAPI(REFIID iid) {
    PublicAPI api = PublicAPI::D3D12;
    if (IsEqualIID(IID_ID3D11ShaderReflection_43, iid))
      api = PublicAPI::D3D11_43;
    else if (IsEqualIID(IID_ID3D11ShaderReflection_47, iid))
      api = PublicAPI::D3D11_47;
    return api;
  }
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxilShaderReflection)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    HRESULT hr = DoBasicQueryInterface<ID3D12ShaderReflection>(this, iid, ppvObject);
    if (hr == E_NOINTERFACE) {
      // ID3D11ShaderReflection is identical to ID3D12ShaderReflection, except
      // for some shorter data structures in some out parameters.
      PublicAPI api = IIDToAPI(iid);
      if (api == m_PublicAPI) {
        *ppvObject = (ID3D12ShaderReflection *)this;
        this->AddRef();
        hr = S_OK;
      }
    }
    return hr;
  }

  HRESULT Load(IDxcBlob *pBlob, const DxilPartHeader *pPart);

  // ID3D12ShaderReflection
  STDMETHODIMP GetDesc(THIS_ _Out_ D3D12_SHADER_DESC *pDesc);

  STDMETHODIMP_(ID3D12ShaderReflectionConstantBuffer*) GetConstantBufferByIndex(THIS_ _In_ UINT Index);
  STDMETHODIMP_(ID3D12ShaderReflectionConstantBuffer*) GetConstantBufferByName(THIS_ _In_ LPCSTR Name);

  STDMETHODIMP GetResourceBindingDesc(THIS_ _In_ UINT ResourceIndex,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc);

  STDMETHODIMP GetInputParameterDesc(THIS_ _In_ UINT ParameterIndex,
    _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc);
  STDMETHODIMP GetOutputParameterDesc(THIS_ _In_ UINT ParameterIndex,
    _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc);
  STDMETHODIMP GetPatchConstantParameterDesc(THIS_ _In_ UINT ParameterIndex,
    _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc);

  STDMETHODIMP_(ID3D12ShaderReflectionVariable*) GetVariableByName(THIS_ _In_ LPCSTR Name);

  STDMETHODIMP GetResourceBindingDescByName(THIS_ _In_ LPCSTR Name,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc);

  STDMETHODIMP_(UINT) GetMovInstructionCount(THIS);
  STDMETHODIMP_(UINT) GetMovcInstructionCount(THIS);
  STDMETHODIMP_(UINT) GetConversionInstructionCount(THIS);
  STDMETHODIMP_(UINT) GetBitwiseInstructionCount(THIS);

  STDMETHODIMP_(D3D_PRIMITIVE) GetGSInputPrimitive(THIS);
  STDMETHODIMP_(BOOL) IsSampleFrequencyShader(THIS);

  STDMETHODIMP_(UINT) GetNumInterfaceSlots(THIS);
  STDMETHODIMP GetMinFeatureLevel(THIS_ _Out_ enum D3D_FEATURE_LEVEL* pLevel);

  STDMETHODIMP_(UINT) GetThreadGroupSize(THIS_
    _Out_opt_ UINT* pSizeX,
    _Out_opt_ UINT* pSizeY,
    _Out_opt_ UINT* pSizeZ);

  STDMETHODIMP_(UINT64) GetRequiresFlags(THIS);
};

class CFunctionReflection;
class DxilLibraryReflection : public DxilModuleReflection, public ID3D12LibraryReflection {
private:
  DXC_MICROCOM_TM_REF_FIELDS()

  // Storage, and function by name:
  typedef DenseMap<StringRef, std::unique_ptr<CFunctionReflection> > FunctionMap;
  typedef DenseMap<const Function*, CFunctionReflection*> FunctionsByPtr;
  FunctionMap m_FunctionMap;
  FunctionsByPtr m_FunctionsByPtr;
  // Enable indexing into functions in deterministic order:
  std::vector<CFunctionReflection*> m_FunctionVector;

  void AddResourceUseToFunctions(DxilResourceBase &resource, unsigned resIndex);
  void AddResourceDependencies();

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxilLibraryReflection)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<ID3D12LibraryReflection>(this, iid, ppvObject);
  }

  HRESULT Load(IDxcBlob *pBlob, const DxilPartHeader *pPart);

  // ID3D12LibraryReflection
  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_LIBRARY_DESC * pDesc);

  STDMETHOD_(ID3D12FunctionReflection *, GetFunctionByIndex)(THIS_ _In_ INT FunctionIndex);
};

_Use_decl_annotations_
HRESULT DxilContainerReflection::Load(IDxcBlob *pContainer) {
  if (pContainer == nullptr) {
    m_container.Release();
    m_pHeader = nullptr;
    m_headerLen = 0;
    return S_OK;
  }

  uint32_t bufLen = pContainer->GetBufferSize();
  const DxilContainerHeader *pHeader =
      IsDxilContainerLike(pContainer->GetBufferPointer(), bufLen);
  if (pHeader == nullptr) {
    return E_INVALIDARG;
  }
  if (!IsValidDxilContainer(pHeader, bufLen)) {
    return E_INVALIDARG;
  }

  m_container = pContainer;
  m_headerLen = bufLen;
  m_pHeader = pHeader;

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartCount(UINT32 *pResult) {
  if (pResult == nullptr) return E_POINTER;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  *pResult = m_pHeader->PartCount;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartKind(UINT32 idx, _Out_ UINT32 *pResult) {
  if (pResult == nullptr) return E_POINTER;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  if (idx >= m_pHeader->PartCount) return E_BOUNDS;
  const DxilPartHeader *pPart = GetDxilContainerPart(m_pHeader, idx);
  *pResult = pPart->PartFourCC;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartContent(UINT32 idx, _COM_Outptr_ IDxcBlob **ppResult) {
  if (ppResult == nullptr) return E_POINTER;
  *ppResult = nullptr;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  if (idx >= m_pHeader->PartCount) return E_BOUNDS;
  const DxilPartHeader *pPart = GetDxilContainerPart(m_pHeader, idx);
  const char *pData = GetDxilPartData(pPart);
  uint32_t offset = (uint32_t)(pData - (char*)m_container->GetBufferPointer()); // Offset from the beginning.
  uint32_t length = pPart->PartSize;
  DxcThreadMalloc TM(m_pMalloc);
  return DxcCreateBlobFromBlob(m_container, offset, length, ppResult);
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::FindFirstPartKind(UINT32 kind, _Out_ UINT32 *pResult) {
  if (pResult == nullptr) return E_POINTER;
  *pResult = 0;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  DxilPartIterator it = std::find_if(begin(m_pHeader), end(m_pHeader), DxilPartIsType(kind));
  if (it == end(m_pHeader)) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  *pResult = it.index;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartReflection(UINT32 idx, REFIID iid, void **ppvObject) {
  if (ppvObject == nullptr) return E_POINTER;
  *ppvObject = nullptr;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  if (idx >= m_pHeader->PartCount) return E_BOUNDS;
  const DxilPartHeader *pPart = GetDxilContainerPart(m_pHeader, idx);
  if (pPart->PartFourCC != DFCC_DXIL && pPart->PartFourCC != DFCC_ShaderDebugInfoDXIL) {
    return E_NOTIMPL;
  }
  
  DxcThreadMalloc TM(m_pMalloc);
  HRESULT hr = S_OK;
  const DxilProgramHeader *pProgramHeader =
    reinterpret_cast<const DxilProgramHeader*>(GetDxilPartData(pPart));
  if (!IsValidDxilProgramHeader(pProgramHeader, pPart->PartSize)) {
    return E_INVALIDARG;
  }

  DXIL::ShaderKind SK = GetVersionShaderType(pProgramHeader->ProgramVersion);
  if (SK == DXIL::ShaderKind::Library) {
    CComPtr<DxilLibraryReflection> pReflection = DxilLibraryReflection::Alloc(m_pMalloc);
    IFCOOM(pReflection.p);
    IFC(pReflection->Load(m_container, pPart));
    IFC(pReflection.p->QueryInterface(iid, ppvObject));
  } else {
    CComPtr<DxilShaderReflection> pReflection = DxilShaderReflection::Alloc(m_pMalloc);
    IFCOOM(pReflection.p);
    PublicAPI api = DxilShaderReflection::IIDToAPI(iid);
    pReflection->SetPublicAPI(api);

    IFC(pReflection->Load(m_container, pPart));
    IFC(pReflection.p->QueryInterface(iid, ppvObject));
  }

Cleanup:
  return hr;
}

void hlsl::CreateDxcContainerReflection(IDxcContainerReflection **ppResult) {
  CComPtr<DxilContainerReflection> pReflection = DxilContainerReflection::Alloc(DxcGetThreadMallocNoRef());
  *ppResult = pReflection.Detach();
  if (*ppResult == nullptr) throw std::bad_alloc();
}

///////////////////////////////////////////////////////////////////////////////
// DxilShaderReflection implementation - helper objects.                     //

class CShaderReflectionType;
class CShaderReflectionVariable;
class CShaderReflectionConstantBuffer;
class CShaderReflection;
struct D3D11_INTERNALSHADER_RESOURCE_DEF;
class CShaderReflectionType : public ID3D12ShaderReflectionType
{
protected:
  D3D12_SHADER_TYPE_DESC              m_Desc;
  std::string                         m_Name;
  std::vector<StringRef>              m_MemberNames;
  std::vector<CShaderReflectionType*> m_MemberTypes;
  CShaderReflectionType*              m_pSubType;
  CShaderReflectionType*              m_pBaseClass;
  std::vector<CShaderReflectionType*> m_Interfaces;
  ULONG_PTR                           m_Identity;

public:
  // Internal
  HRESULT Initialize(
    DxilModule              &M,
    llvm::Type              *type,
    DxilFieldAnnotation     &typeAnnotation,
    unsigned int            baseOffset,
    std::vector<std::unique_ptr<CShaderReflectionType>>& allTypes);

  // ID3D12ShaderReflectionType
  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc);

  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByIndex)(UINT Index);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByName)(LPCSTR Name);
  STDMETHOD_(LPCSTR, GetMemberTypeName)(UINT Index);

  STDMETHOD(IsEqual)(THIS_ ID3D12ShaderReflectionType* pType);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetSubType)(THIS);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetBaseClass)(THIS);
  STDMETHOD_(UINT, GetNumInterfaces)(THIS);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetInterfaceByIndex)(THIS_ UINT uIndex);
  STDMETHOD(IsOfType)(THIS_ ID3D12ShaderReflectionType* pType);
  STDMETHOD(ImplementsInterface)(THIS_ ID3D12ShaderReflectionType* pBase);

  bool CheckEqual(_In_ CShaderReflectionType *pOther) {
    return m_Identity == pOther->m_Identity;
  }
};

class CShaderReflectionVariable : public ID3D12ShaderReflectionVariable
{
protected:
  D3D12_SHADER_VARIABLE_DESC          m_Desc;
  CShaderReflectionType              *m_pType;
  CShaderReflectionConstantBuffer    *m_pBuffer;
  BYTE                               *m_pDefaultValue;

public:
  void Initialize(CShaderReflectionConstantBuffer *pBuffer,
                  D3D12_SHADER_VARIABLE_DESC *pDesc,
                  CShaderReflectionType *pType, BYTE *pDefaultValue);

  LPCSTR GetName() { return m_Desc.Name; }

  // ID3D12ShaderReflectionVariable
  STDMETHOD(GetDesc)(D3D12_SHADER_VARIABLE_DESC *pDesc);

  STDMETHOD_(ID3D12ShaderReflectionType*, GetType)();
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer*, GetBuffer)();

  STDMETHOD_(UINT, GetInterfaceSlot)(THIS_ UINT uArrayIndex);
};

class CShaderReflectionConstantBuffer : public ID3D12ShaderReflectionConstantBuffer
{
protected:
  D3D12_SHADER_BUFFER_DESC                m_Desc;
  std::vector<CShaderReflectionVariable>  m_Variables;

public:
  CShaderReflectionConstantBuffer() = default;
  CShaderReflectionConstantBuffer(CShaderReflectionConstantBuffer &&other) {
    m_Desc = other.m_Desc;
    std::swap(m_Variables, other.m_Variables);
  }

  void Initialize(DxilModule &M,
                  DxilCBuffer &CB,
                  std::vector<std::unique_ptr<CShaderReflectionType>>& allTypes);
  void InitializeStructuredBuffer(DxilModule &M,
                                  DxilResource &R,
                                  std::vector<std::unique_ptr<CShaderReflectionType>>& allTypes);
  LPCSTR GetName() { return m_Desc.Name; }

  // ID3D12ShaderReflectionConstantBuffer
  STDMETHOD(GetDesc)(D3D12_SHADER_BUFFER_DESC *pDesc);

  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByIndex)(UINT Index);
  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByName)(LPCSTR Name);
};

// Invalid type sentinel definitions
class CInvalidSRType;
class CInvalidSRVariable;
class CInvalidSRConstantBuffer;
class CInvalidSRLibraryFunction;
class CInvalidSRFunctionParameter;

class CInvalidSRType : public ID3D12ShaderReflectionType {
  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByIndex)(UINT Index);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByName)(LPCSTR Name);
  STDMETHOD_(LPCSTR, GetMemberTypeName)(UINT Index) { return "$Invalid"; }

  STDMETHOD(IsEqual)(THIS_ ID3D12ShaderReflectionType* pType) { return E_FAIL; }
  STDMETHOD_(ID3D12ShaderReflectionType*, GetSubType)(THIS);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetBaseClass)(THIS);
  STDMETHOD_(UINT, GetNumInterfaces)(THIS) { return 0; }
  STDMETHOD_(ID3D12ShaderReflectionType*, GetInterfaceByIndex)(THIS_ UINT uIndex);
  STDMETHOD(IsOfType)(THIS_ ID3D12ShaderReflectionType* pType) { return E_FAIL; }
  STDMETHOD(ImplementsInterface)(THIS_ ID3D12ShaderReflectionType* pBase) { return E_FAIL; }
};
static CInvalidSRType g_InvalidSRType;

ID3D12ShaderReflectionType* CInvalidSRType::GetMemberTypeByIndex(UINT) { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetMemberTypeByName(LPCSTR) { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetSubType() { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetBaseClass() { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetInterfaceByIndex(UINT) { return &g_InvalidSRType; }

class CInvalidSRVariable : public ID3D12ShaderReflectionVariable {
  STDMETHOD(GetDesc)(D3D12_SHADER_VARIABLE_DESC *pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionType*, GetType)() { return &g_InvalidSRType; }
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer*, GetBuffer)();

  STDMETHOD_(UINT, GetInterfaceSlot)(THIS_ UINT uIndex) { return UINT_MAX; }
};
static CInvalidSRVariable g_InvalidSRVariable;

class CInvalidSRConstantBuffer : public ID3D12ShaderReflectionConstantBuffer {
  STDMETHOD(GetDesc)(D3D12_SHADER_BUFFER_DESC *pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByIndex)(UINT Index) { return &g_InvalidSRVariable; }
  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByName)(LPCSTR Name) { return &g_InvalidSRVariable; }
};
static CInvalidSRConstantBuffer g_InvalidSRConstantBuffer;

class CInvalidFunctionParameter : public ID3D12FunctionParameterReflection {
  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_PARAMETER_DESC * pDesc) { return E_FAIL; }
};
CInvalidFunctionParameter g_InvalidFunctionParameter;

class CInvalidFunction : public ID3D12FunctionReflection {
  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_FUNCTION_DESC * pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByIndex)(THIS_ _In_ UINT BufferIndex) { return &g_InvalidSRConstantBuffer; }
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByName)(THIS_ _In_ LPCSTR Name) { return &g_InvalidSRConstantBuffer; }

  STDMETHOD(GetResourceBindingDesc)(THIS_ _In_ UINT ResourceIndex,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC * pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionVariable *, GetVariableByName)(THIS_ _In_ LPCSTR Name) { return nullptr; }

  STDMETHOD(GetResourceBindingDescByName)(THIS_ _In_ LPCSTR Name,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC * pDesc) { return E_FAIL; }

  // Use D3D_RETURN_PARAMETER_INDEX to get description of the return value.
  STDMETHOD_(ID3D12FunctionParameterReflection *, GetFunctionParameter)(THIS_ _In_ INT ParameterIndex) { return &g_InvalidFunctionParameter; }
};
CInvalidFunction g_InvalidFunction;

void CShaderReflectionVariable::Initialize(
    CShaderReflectionConstantBuffer *pBuffer, D3D12_SHADER_VARIABLE_DESC *pDesc,
    CShaderReflectionType *pType, BYTE *pDefaultValue) {
  m_pBuffer = pBuffer;
  memcpy(&m_Desc, pDesc, sizeof(m_Desc));
  m_pType = pType;
  m_pDefaultValue = pDefaultValue;
}

HRESULT CShaderReflectionVariable::GetDesc(D3D12_SHADER_VARIABLE_DESC *pDesc) {
  if (!pDesc) return E_POINTER;
  memcpy(pDesc, &m_Desc, sizeof(m_Desc));
  return S_OK;
}

ID3D12ShaderReflectionType *CShaderReflectionVariable::GetType() {
  return m_pType;
}

ID3D12ShaderReflectionConstantBuffer *CShaderReflectionVariable::GetBuffer() {
  return m_pBuffer;
}

UINT CShaderReflectionVariable::GetInterfaceSlot(UINT uArrayIndex) {
  return UINT_MAX;
}

ID3D12ShaderReflectionConstantBuffer *CInvalidSRVariable::GetBuffer() {
  return &g_InvalidSRConstantBuffer;
}

STDMETHODIMP CShaderReflectionType::GetDesc(D3D12_SHADER_TYPE_DESC *pDesc)
{
  if (!pDesc) return E_POINTER;
  memcpy(pDesc, &m_Desc, sizeof(m_Desc));
  return S_OK;
}

STDMETHODIMP_(ID3D12ShaderReflectionType*) CShaderReflectionType::GetMemberTypeByIndex(UINT Index)
{
  if (Index >= m_MemberTypes.size()) {
    return &g_InvalidSRType;
  }
  return m_MemberTypes[Index];
}

STDMETHODIMP_(LPCSTR) CShaderReflectionType::GetMemberTypeName(UINT Index)
{
  if (Index >= m_MemberTypes.size()) {
    return nullptr;
  }
  return (LPCSTR) m_MemberNames[Index].bytes_begin();
}

STDMETHODIMP_(ID3D12ShaderReflectionType*) CShaderReflectionType::GetMemberTypeByName(LPCSTR Name)
{
  UINT memberCount = m_Desc.Members;
  for( UINT mm = 0; mm < memberCount; ++mm ) {
    if( m_MemberNames[mm] == Name ) {
      return m_MemberTypes[mm];
    }
  }
  return nullptr;
}

STDMETHODIMP CShaderReflectionType::IsEqual(THIS_ ID3D12ShaderReflectionType* pType)
{
  // TODO: implement this check, if users actually depend on it
  return S_FALSE;
}

STDMETHODIMP_(ID3D12ShaderReflectionType*) CShaderReflectionType::GetSubType(THIS)
{
  // TODO: implement `class`-related features, if requested
  return nullptr;
}

STDMETHODIMP_(ID3D12ShaderReflectionType*) CShaderReflectionType::GetBaseClass(THIS)
{
  // TODO: implement `class`-related features, if requested
  return nullptr;
}

STDMETHODIMP_(UINT) CShaderReflectionType::GetNumInterfaces(THIS)
{
  // HLSL interfaces have been deprecated
  return 0;
}

STDMETHODIMP_(ID3D12ShaderReflectionType*) CShaderReflectionType::GetInterfaceByIndex(THIS_ UINT uIndex)
{
  // HLSL interfaces have been deprecated
  return nullptr;
}

STDMETHODIMP CShaderReflectionType::IsOfType(THIS_ ID3D12ShaderReflectionType* pType)
{
  // TODO: implement `class`-related features, if requested
  return S_FALSE;
}

STDMETHODIMP CShaderReflectionType::ImplementsInterface(THIS_ ID3D12ShaderReflectionType* pBase)
{
  // HLSL interfaces have been deprecated
  return S_FALSE;
}

// Helper routine for types that don't have an obvious mapping
// to the existing shader reflection interface.
static bool ProcessUnhandledObjectType(
  llvm::StructType            *structType,
  D3D_SHADER_VARIABLE_TYPE    *outObjectType)
{
  // Don't actually make this a hard error, but instead report the problem using a suitable debug message.
#ifdef DBG
  OutputDebugFormatA("DxilContainerReflection.cpp: error: unhandled object type '%s'.\n", structType->getName().str().c_str());
#endif
  *outObjectType = D3D_SVT_VOID;
  return true;
}

// Helper routine to try to detect if a type represents an HLSL "object" type
// (a texture, sampler, buffer, etc.), and to extract the coresponding shader
// reflection type.
static bool TryToDetectObjectType(
  llvm::StructType            *structType,
  D3D_SHADER_VARIABLE_TYPE    *outObjectType)
{
  // Note: This logic is largely duplicated from `dxilutil::IsHLSLObjectType`
  // with the addition of returning the appropriate reflection type tag.
  //
  // That logic looks error-prone, since it relies on string tests against
  // type names, including cases that just test against a prefix.
  // This code doesn't try to be any more robust.

  StringRef name = structType->getName();

  if(name.startswith("dx.types.wave_t") )
  {
    return ProcessUnhandledObjectType(structType, outObjectType);
  }

  // Strip off some prefixes we are likely to see.
  name = name.ltrim("class.");
  name = name.ltrim("struct.");

  // Slice types occur as intermediates (they aren not objects)
  if(name.endswith("_slice_type")) { return false; }

  // We might check for an exact name match, or a prefix match
#define EXACT_MATCH(NAME, TAG) \
  else if(name == #NAME) do { *outObjectType = TAG; return true; } while(0)
#define PREFIX_MATCH(NAME, TAG) \
  else if(name.startswith(#NAME)) do { *outObjectType = TAG; return true; } while(0)

  if(0) {}
  EXACT_MATCH(SamplerState,               D3D_SVT_SAMPLER);
  EXACT_MATCH(SamplerComparisonState,     D3D_SVT_SAMPLER);

  // Note: GS output stream types are supported in the reflection interface.
  else if(name.startswith("TriangleStream"))    { return ProcessUnhandledObjectType(structType, outObjectType); }
  else if(name.startswith("PointStream"))       { return ProcessUnhandledObjectType(structType, outObjectType); }
  else if(name.startswith("LineStream"))        { return ProcessUnhandledObjectType(structType, outObjectType); }

  PREFIX_MATCH(AppendStructuredBuffer,    D3D_SVT_APPEND_STRUCTURED_BUFFER);
  PREFIX_MATCH(ConsumeStructuredBuffer,   D3D_SVT_CONSUME_STRUCTURED_BUFFER);
  PREFIX_MATCH(ConstantBuffer,            D3D_SVT_CBUFFER);

  // Note: the `HLModule` code does this trick to avoid checking more names
  // than it has to, but it doesn't seem 100% correct to do this.
  // TODO: consider just listing the `RasterizerOrdered` cases explicitly,
  // just as we do for the `RW` cases already.
  name = name.ltrim("RasterizerOrdered");

  if(0) {}
  EXACT_MATCH(ByteAddressBuffer,          D3D_SVT_BYTEADDRESS_BUFFER);
  EXACT_MATCH(RWByteAddressBuffer,        D3D_SVT_RWBYTEADDRESS_BUFFER);
  PREFIX_MATCH(Buffer,                    D3D_SVT_BUFFER);
  PREFIX_MATCH(RWBuffer,                  D3D_SVT_RWBUFFER);
  PREFIX_MATCH(StructuredBuffer,          D3D_SVT_STRUCTURED_BUFFER);
  PREFIX_MATCH(RWStructuredBuffer,        D3D_SVT_RWSTRUCTURED_BUFFER);
  PREFIX_MATCH(Texture1D,                 D3D_SVT_TEXTURE1D);
  PREFIX_MATCH(RWTexture1D,               D3D_SVT_RWTEXTURE1D);
  PREFIX_MATCH(Texture1DArray,            D3D_SVT_TEXTURE1DARRAY);
  PREFIX_MATCH(RWTexture1DArray,          D3D_SVT_RWTEXTURE1DARRAY);
  PREFIX_MATCH(Texture2D,                 D3D_SVT_TEXTURE2D);
  PREFIX_MATCH(RWTexture2D,               D3D_SVT_RWTEXTURE2D);
  PREFIX_MATCH(Texture2DArray,            D3D_SVT_TEXTURE2DARRAY);
  PREFIX_MATCH(RWTexture2DArray,          D3D_SVT_RWTEXTURE2DARRAY);
  PREFIX_MATCH(Texture3D,                 D3D_SVT_TEXTURE3D);
  PREFIX_MATCH(RWTexture3D,               D3D_SVT_RWTEXTURE3D);
  PREFIX_MATCH(TextureCube,               D3D_SVT_TEXTURECUBE);
  PREFIX_MATCH(TextureCubeArray,          D3D_SVT_TEXTURECUBEARRAY);
  PREFIX_MATCH(Texture2DMS,               D3D_SVT_TEXTURE2DMS);
  PREFIX_MATCH(Texture2DMSArray,          D3D_SVT_TEXTURE2DMSARRAY);

#undef EXACT_MATCH
#undef PREFIX_MATCH

  // Default: not an object type
  return false;
}

// Helper to determine if an LLVM type represents an HLSL
// object type (uses the `TryToDetectObjectType()` function
// defined previously).
static bool IsObjectType(
  llvm::Type* inType)
{
  llvm::Type* type = inType;
  while(type->isArrayTy())
  {
    type = type->getArrayElementType();
  }

  llvm::StructType* structType = dyn_cast<StructType>(type);
  if(!structType)
    return false;

  D3D_SHADER_VARIABLE_TYPE ignored;
  return TryToDetectObjectType(structType, &ignored);
}

// Main logic for translating an LLVM type and associated
// annotations into a D3D shader reflection type.
HRESULT CShaderReflectionType::Initialize(
  DxilModule              &M,
  llvm::Type              *inType,
  DxilFieldAnnotation     &typeAnnotation,
  unsigned int            baseOffset,
  std::vector<std::unique_ptr<CShaderReflectionType>>& allTypes)
{
  DXASSERT_NOMSG(inType);

  // Set a bunch of fields to default values, to avoid duplication.
  m_Desc.Rows = 0;
  m_Desc.Columns = 0;
  m_Desc.Elements = 0;
  m_Desc.Members = 0;

  // Extract offset relative to parent.
  // Note: the `baseOffset` is used in the case where the type in
  // question is a field in a constant buffer, since then both the
  // field and the variable store the same offset information, and
  // we need to zero out the value in the type to avoid the user
  // of the reflection interface seeing 2x the correct value.
  m_Desc.Offset = typeAnnotation.GetCBufferOffset() - baseOffset;

  // Arrays don't seem to be represented directly in the reflection
  // data, but only as the `Elements` field being non-zero.
  // We "unwrap" any array type here, and then proceed to look
  // at the element type.
  llvm::Type* type = inType;

  while(type->isArrayTy())
  {
    llvm::Type* elementType = type->getArrayElementType();

    // Note: At this point an HLSL matrix type may appear as an ordinary
    // array (not wrapped in a `struct`), so `HLMatrixLower::IsMatrixType()`
    // is not sufficient. Instead we need to check the field annotation.
    //
    // We might have an array of matrices, though, so we only exit if
    // the field annotation says we have a matrix, and we've bottomed
    // out and the element type isn't itself an array.
    if(typeAnnotation.HasMatrixAnnotation() && !elementType->isArrayTy())
    {
      break;
    }

    // Non-array types should have `Elements` be zero, so as soon as we
    // find that we have our first real array (not a matrix), we initialize `Elements`
    if(!m_Desc.Elements) m_Desc.Elements = 1;

    // It isn't clear what is the desired behavior for multi-dimensional arrays,
    // but for now we do the expedient thing of multiplying out all their
    // dimensions.
    m_Desc.Elements *= type->getArrayNumElements();
    type = elementType;
  }

  // Default to a scalar type, just to avoid some duplication later.
  m_Desc.Class = D3D_SVC_SCALAR;

  // Look at the annotation to try to determine the basic type of value.
  //
  // Note that DXIL supports some types that don't currently have equivalents
  // in the reflection interface, so we try to muddle through here.
  D3D_SHADER_VARIABLE_TYPE componentType = D3D_SVT_VOID;
  switch(typeAnnotation.GetCompType().GetKind())
  {
  case hlsl::DXIL::ComponentType::Invalid:
    break;

  case hlsl::DXIL::ComponentType::I1:
    componentType = D3D_SVT_BOOL;
    m_Name = "bool";
    break;

  case hlsl::DXIL::ComponentType::I16:
    componentType = D3D_SVT_MIN16INT;
    m_Name = "min16int";
    break;

  case hlsl::DXIL::ComponentType::U16:
    componentType = D3D_SVT_MIN16UINT;
    m_Name = "min16uint";
    break;

  case hlsl::DXIL::ComponentType::I64:
#ifdef DBG
    OutputDebugStringA("DxilContainerReflection.cpp: warning: component of type 'I64' being reflected as if 'I32'\n");
#endif
  case hlsl::DXIL::ComponentType::I32:
    componentType = D3D_SVT_INT;
    m_Name = "int";
    break;

  case hlsl::DXIL::ComponentType::U64:
#ifdef DBG
    OutputDebugStringA("DxilContainerReflection.cpp: warning: component of type 'U64' being reflected as if 'U32'\n");
#endif
  case hlsl::DXIL::ComponentType::U32:
    componentType = D3D_SVT_UINT;
    m_Name = "uint";
    break;

  case hlsl::DXIL::ComponentType::F16:
  case hlsl::DXIL::ComponentType::SNormF16:
  case hlsl::DXIL::ComponentType::UNormF16:
    componentType = D3D_SVT_MIN16FLOAT;
    m_Name = "min16float";
    break;

  case hlsl::DXIL::ComponentType::F32:
  case hlsl::DXIL::ComponentType::SNormF32:
  case hlsl::DXIL::ComponentType::UNormF32:
    componentType = D3D_SVT_FLOAT;
    m_Name = "float";
    break;

  case hlsl::DXIL::ComponentType::F64:
  case hlsl::DXIL::ComponentType::SNormF64:
  case hlsl::DXIL::ComponentType::UNormF64:
    componentType = D3D_SVT_DOUBLE;
    m_Name = "double";
    break;

  default:
#ifdef DBG
    OutputDebugStringA("DxilContainerReflection.cpp: error: unknown component type\n");
#endif
    break;
  }
  m_Desc.Type = componentType;

  // A matrix type is encoded as a vector type, plus annotations, so we
  // need to check for this case before other vector cases.
  if(typeAnnotation.HasMatrixAnnotation())
  {
    // We can extract the details from the annotation.
    DxilMatrixAnnotation const& matrixAnnotation = typeAnnotation.GetMatrixAnnotation();

    switch(matrixAnnotation.Orientation)
    {
    default:
#ifdef DBG
      OutputDebugStringA("DxilContainerReflection.cpp: error: unknown matrix orientation\n");
#endif
    // Note: column-major layout is the default
    case hlsl::MatrixOrientation::Undefined:
    case hlsl::MatrixOrientation::ColumnMajor:
      m_Desc.Class = D3D_SVC_MATRIX_COLUMNS;
      break;

    case hlsl::MatrixOrientation::RowMajor:
      m_Desc.Class = D3D_SVC_MATRIX_ROWS;
      break;
    }

    m_Desc.Rows = matrixAnnotation.Rows;
    m_Desc.Columns = matrixAnnotation.Cols;
    m_Name += std::to_string(matrixAnnotation.Rows) + "x" + std::to_string(matrixAnnotation.Cols);
  }
  else if( type->isVectorTy() )
  {
    // We assume that LLVM vectors either represent matrices (handled above)
    // or HLSL vectors.
    //
    // Note: the reflection interface encodes an N-vector as if it had 1 row
    // and N columns.
    m_Desc.Class = D3D_SVC_VECTOR;
    m_Desc.Rows = 1;
    m_Desc.Columns = type->getVectorNumElements();

    m_Name += std::to_string(type->getVectorNumElements());
  }
  else if( type->isStructTy() )
  {
    // A struct type might be an ordinary user-defined `struct`,
    // or one of the builtin in HLSL "object" types.
    StructType *structType = cast<StructType>(type);

    // We use our function to try to detect an object type
    // based on its name.
    if(TryToDetectObjectType(structType, &m_Desc.Type))
    {
      m_Desc.Class = D3D_SVC_OBJECT;
    }
    else
    {
      // Otherwise we have a struct and need to recurse on its fields.
      m_Desc.Class = D3D_SVC_STRUCT;
      m_Desc.Rows = 1;

      // Try to "clean" the type name for use in reflection data
      llvm::StringRef name = structType->getName();
      name = name.ltrim("dx.alignment.legacy.");
      name = name.ltrim("struct.");
      m_Name = name;

      // Fields may have annotations, and we need to look at these
      // in order to decode their types properly.
      DxilTypeSystem &typeSys = M.GetTypeSystem();
      DxilStructAnnotation *structAnnotation = typeSys.GetStructAnnotation(structType);

      // There is no annotation for empty structs
      unsigned int fieldCount = 0;
      if (structAnnotation)
        fieldCount = type->getStructNumElements();

      // The DXBC reflection info computes `Columns` for a
      // `struct` type from the fields (see below)
      UINT columnCounter = 0;

      for(unsigned int ff = 0; ff < fieldCount; ++ff)
      {
        DxilFieldAnnotation& fieldAnnotation = structAnnotation->GetFieldAnnotation(ff);
        llvm::Type* fieldType = structType->getStructElementType(ff);

        // Skip fields with object types, since applications may not expect to see them here.
        //
        // TODO: should skipping be context-dependent, since we might not be inside
        // a constant buffer?
        if( IsObjectType(fieldType) )
        {
          continue;
        }

        CShaderReflectionType *fieldReflectionType = new CShaderReflectionType();
        allTypes.push_back(std::unique_ptr<CShaderReflectionType>(fieldReflectionType));

        fieldReflectionType->Initialize(M, fieldType, fieldAnnotation, 0, allTypes);

        m_MemberTypes.push_back(fieldReflectionType);
        m_MemberNames.push_back(fieldAnnotation.GetFieldName().c_str());

        // Effectively, we want to add one to `Columns` for every scalar nested recursively
        // inside this `struct` type (ignoring objects, which we filtered above). We should
        // be able to compute this as the product of the `Columns`, `Rows` and `Elements`
        // of each field, with the caveat that some of these may be zero, but shoud be
        // treated as one.
        columnCounter +=
            (fieldReflectionType->m_Desc.Columns  ? fieldReflectionType->m_Desc.Columns  : 1)
          * (fieldReflectionType->m_Desc.Rows     ? fieldReflectionType->m_Desc.Rows     : 1)
          * (fieldReflectionType->m_Desc.Elements ? fieldReflectionType->m_Desc.Elements : 1);
      }

      m_Desc.Columns = columnCounter;

      // Because we might have skipped fields during enumeration,
      // the `Members` count in the description might not be the same
      // as the field count of the original LLVM type.
      m_Desc.Members = m_MemberTypes.size();
    }
  }
  else if( type->isPointerTy() )
  {
#ifdef DBG
      OutputDebugStringA("DxilContainerReflection.cpp: error: cannot reflect pointer type\n");
#endif
  }
  else if( type->isVoidTy() )
  {
    // Name for `void` wasn't handle in the component-type `switch` above
    m_Name = "void";
    m_Desc.Class = D3D_SVC_SCALAR;
    m_Desc.Rows = 1;
    m_Desc.Columns = 1;
  }
  else
  {
    // Assume we have a scalar at this point.
    m_Desc.Class = D3D_SVC_SCALAR;
    m_Desc.Rows = 1;
    m_Desc.Columns = 1;

    // Special-case naming
    switch(m_Desc.Type)
    {
    default:
      break;

    case D3D_SVT_UINT:
      // Scalar `uint` gets reflected as `dword`, while vectors/matrices use `uint`...
      m_Name = "dword";
      break;
    }
  }
  // TODO: are there other cases to be handled?

  m_Desc.Name = m_Name.c_str();

  return S_OK;
}


void CShaderReflectionConstantBuffer::Initialize(
  DxilModule &M,
  DxilCBuffer &CB,
  std::vector<std::unique_ptr<CShaderReflectionType>>& allTypes) {
  ZeroMemory(&m_Desc, sizeof(m_Desc));
  m_Desc.Name = CB.GetGlobalName().c_str();
  m_Desc.Size = CB.GetSize() / CB.GetRangeSize();
  m_Desc.Size = (m_Desc.Size + 0x0f) & ~(0x0f); // Round up to 16 bytes for reflection.
  m_Desc.Type = D3D_CT_CBUFFER;
  m_Desc.uFlags = 0;
  Type *Ty = CB.GetGlobalSymbol()->getType()->getPointerElementType();
  // For ConstantBuffer<> buf[2], the array size is in Resource binding count
  // part.
  if (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();

  DxilTypeSystem &typeSys = M.GetTypeSystem();
  StructType *ST = cast<StructType>(Ty);
  DxilStructAnnotation *annotation =
      typeSys.GetStructAnnotation(cast<StructType>(ST));
  // Dxil from dxbc doesn't have annotation.
  if (!annotation)
    return;

  m_Desc.Variables = ST->getNumContainedTypes();
  unsigned lastIndex = ST->getNumContainedTypes() - 1;

  for (unsigned i = 0; i < ST->getNumContainedTypes(); ++i) {
    DxilFieldAnnotation &fieldAnnotation = annotation->GetFieldAnnotation(i);

    D3D12_SHADER_VARIABLE_DESC VarDesc;
    ZeroMemory(&VarDesc, sizeof(VarDesc));
    VarDesc.uFlags |= D3D_SVF_USED; // Will update in SetCBufferUsage.
    CShaderReflectionVariable Var;
    //Create reflection type.
    CShaderReflectionType *pVarType = new CShaderReflectionType();
    allTypes.push_back(std::unique_ptr<CShaderReflectionType>(pVarType));
    pVarType->Initialize(M, ST->getContainedType(i), fieldAnnotation, fieldAnnotation.GetCBufferOffset(), allTypes);

    BYTE *pDefaultValue = nullptr;

    VarDesc.Name = fieldAnnotation.GetFieldName().c_str();
    VarDesc.StartOffset = fieldAnnotation.GetCBufferOffset();
    if (i < lastIndex) {
      DxilFieldAnnotation &nextFieldAnnotation =
          annotation->GetFieldAnnotation(i + 1);
      VarDesc.Size = nextFieldAnnotation.GetCBufferOffset() - fieldAnnotation.GetCBufferOffset();
    }
    else {
      VarDesc.Size = CB.GetSize() - fieldAnnotation.GetCBufferOffset();
    }
    Var.Initialize(this, &VarDesc, pVarType, pDefaultValue);
    m_Variables.push_back(Var);
  }
}

static unsigned CalcTypeSize(Type *Ty) {
  // Assume aligned values.
  if (Ty->isIntegerTy() || Ty->isFloatTy()) {
    return Ty->getPrimitiveSizeInBits() / 8;
  }
  else if (Ty->isArrayTy()) {
    ArrayType *AT = dyn_cast<ArrayType>(Ty);
    return AT->getNumElements() * CalcTypeSize(AT->getArrayElementType());
  }
  else if (Ty->isStructTy()) {
    StructType *ST = dyn_cast<StructType>(Ty);
    unsigned i = 0, c = ST->getStructNumElements();
    unsigned result = 0;
    for (; i < c; ++i) {
      result += CalcTypeSize(ST->getStructElementType(i));
      // TODO: align!
    }
    return result;
  }
  else if (Ty->isVectorTy()) {
    VectorType *VT = dyn_cast<VectorType>(Ty);
    return VT->getVectorNumElements() * CalcTypeSize(VT->getVectorElementType());
  }
  else {
    DXASSERT_NOMSG(false);
    return 0;
  }
}

static unsigned CalcResTypeSize(DxilModule &M, DxilResource &R) {
  UNREFERENCED_PARAMETER(M);
  Type *Ty = R.GetGlobalSymbol()->getType()->getPointerElementType();
  return CalcTypeSize(Ty);
}

void CShaderReflectionConstantBuffer::InitializeStructuredBuffer(
  DxilModule &M,
  DxilResource &R,
  std::vector<std::unique_ptr<CShaderReflectionType>>& allTypes) {
  ZeroMemory(&m_Desc, sizeof(m_Desc));
  m_Desc.Name = R.GetGlobalName().c_str();
  //m_Desc.Size = R.GetSize();
  m_Desc.Type = D3D11_CT_RESOURCE_BIND_INFO;
  m_Desc.uFlags = 0;
  m_Desc.Variables = 1;

  D3D12_SHADER_VARIABLE_DESC VarDesc;
  ZeroMemory(&VarDesc, sizeof(VarDesc));
  VarDesc.Name = "$Element";
  VarDesc.Size = CalcResTypeSize(M, R); // aligned bytes
  VarDesc.StartTexture = UINT_MAX;
  VarDesc.StartSampler = UINT_MAX;
  VarDesc.uFlags |= D3D_SVF_USED; // TODO: not necessarily true
  CShaderReflectionVariable Var;
  CShaderReflectionType *pVarType = nullptr;

  // Create reflection type, if we have the necessary annotation info

  // Extract the `struct` that wraps element type of the buffer resource
  Constant *GV = R.GetGlobalSymbol();
  Type *Ty = GV->getType()->getPointerElementType();
  if(Ty->isArrayTy())
      Ty = Ty->getArrayElementType();
  StructType *ST = cast<StructType>(Ty);

  // Look up struct type annotation on the element type
  DxilTypeSystem &typeSys = M.GetTypeSystem();
  DxilStructAnnotation *annotation =
    typeSys.GetStructAnnotation(cast<StructType>(ST));

  // Dxil from dxbc doesn't have annotation.
  if(annotation)
  {
    // Actually create the reflection type.
    pVarType = new CShaderReflectionType();
    allTypes.push_back(std::unique_ptr<CShaderReflectionType>(pVarType));

    // The user-visible element type is the first field of the wrapepr `struct`
    Type *fieldType = ST->getElementType(0);
    DxilFieldAnnotation &fieldAnnotation = annotation->GetFieldAnnotation(0);

    pVarType->Initialize(M, fieldType, fieldAnnotation, fieldAnnotation.GetCBufferOffset(), allTypes);
  }

  BYTE *pDefaultValue = nullptr;
  Var.Initialize(this, &VarDesc, pVarType, pDefaultValue);
  m_Variables.push_back(Var);

  m_Desc.Size = VarDesc.Size;
}

HRESULT CShaderReflectionConstantBuffer::GetDesc(D3D12_SHADER_BUFFER_DESC *pDesc) {
  if (!pDesc)
    return E_POINTER;
  memcpy(pDesc, &m_Desc, sizeof(m_Desc));
  return S_OK;
}

ID3D12ShaderReflectionVariable *
CShaderReflectionConstantBuffer::GetVariableByIndex(UINT Index) {
  if (Index >= m_Variables.size()) {
    return &g_InvalidSRVariable;
  }

  return &m_Variables[Index];
}

ID3D12ShaderReflectionVariable *
CShaderReflectionConstantBuffer::GetVariableByName(LPCSTR Name) {
  UINT index;

  if (NULL == Name) {
    return &g_InvalidSRVariable;
  }

  for (index = 0; index < m_Variables.size(); ++index) {
    if (0 == strcmp(m_Variables[index].GetName(), Name)) {
      return &m_Variables[index];
    }
  }

  return &g_InvalidSRVariable;
}

///////////////////////////////////////////////////////////////////////////////
// DxilShaderReflection implementation.                                      //

static DxilResource *DxilResourceFromBase(DxilResourceBase *RB) {
  DxilResourceBase::Class C = RB->GetClass();
  if (C == DXIL::ResourceClass::UAV || C == DXIL::ResourceClass::SRV)
    return (DxilResource *)RB;
  return nullptr;
}

static D3D_SHADER_INPUT_TYPE ResourceToShaderInputType(DxilResourceBase *RB) {
  DxilResource *R = DxilResourceFromBase(RB);
  bool isUAV = RB->GetClass() == DxilResourceBase::Class::UAV;
  switch (RB->GetKind()) {
  case DxilResource::Kind::CBuffer:
    return D3D_SIT_CBUFFER;
  case DxilResource::Kind::Sampler:
    return D3D_SIT_SAMPLER;
  case DxilResource::Kind::RawBuffer:
    return isUAV ? D3D_SIT_UAV_RWBYTEADDRESS : D3D_SIT_BYTEADDRESS;
  case DxilResource::Kind::StructuredBuffer: {
    if (!isUAV) return D3D_SIT_STRUCTURED;
    // TODO: D3D_SIT_UAV_CONSUME_STRUCTURED, D3D_SIT_UAV_APPEND_STRUCTURED?
    if (R->HasCounter()) return D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER;
    return D3D_SIT_UAV_RWSTRUCTURED;
  }
  case DxilResource::Kind::TBuffer:
  case DxilResource::Kind::TypedBuffer:
  case DxilResource::Kind::Texture1D:
  case DxilResource::Kind::Texture1DArray:
  case DxilResource::Kind::Texture2D:
  case DxilResource::Kind::Texture2DArray:
  case DxilResource::Kind::Texture2DMS:
  case DxilResource::Kind::Texture2DMSArray:
  case DxilResource::Kind::Texture3D:
  case DxilResource::Kind::TextureCube:
  case DxilResource::Kind::TextureCubeArray:
    return isUAV ? D3D_SIT_UAV_RWTYPED : D3D_SIT_TEXTURE;
  case DxilResource::Kind::RTAccelerationStructure:
    return (D3D_SHADER_INPUT_TYPE)D3D_SIT_RTACCELERATIONSTRUCTURE;
  default:
    return (D3D_SHADER_INPUT_TYPE)-1;
  }
}

static D3D_RESOURCE_RETURN_TYPE ResourceToReturnType(DxilResourceBase *RB) {
  DxilResource *R = DxilResourceFromBase(RB);
  if (R != nullptr) {
    CompType CT = R->GetCompType();
    if (CT.GetKind() == CompType::Kind::F64) return D3D_RETURN_TYPE_DOUBLE;
    if (CT.IsUNorm()) return D3D_RETURN_TYPE_UNORM;
    if (CT.IsSNorm()) return D3D_RETURN_TYPE_SNORM;
    if (CT.IsSIntTy()) return D3D_RETURN_TYPE_SINT;
    if (CT.IsUIntTy()) return D3D_RETURN_TYPE_UINT;
    if (CT.IsFloatTy()) return D3D_RETURN_TYPE_FLOAT;

    // D3D_RETURN_TYPE_CONTINUED: Return type is a multiple-dword type, such as a
    // double or uint64, and the component is continued from the previous
    // component that was declared. The first component represents the lower bits.
    return D3D_RETURN_TYPE_MIXED;
  }

  return (D3D_RESOURCE_RETURN_TYPE)0;
}

static D3D_SRV_DIMENSION ResourceToDimension(DxilResourceBase *RB) {
  switch (RB->GetKind()) {
  case DxilResource::Kind::StructuredBuffer:
  case DxilResource::Kind::TypedBuffer:
  case DxilResource::Kind::TBuffer:
    return D3D_SRV_DIMENSION_BUFFER;
  case DxilResource::Kind::Texture1D:
    return D3D_SRV_DIMENSION_TEXTURE1D;
  case DxilResource::Kind::Texture1DArray:
    return D3D_SRV_DIMENSION_TEXTURE1DARRAY;
  case DxilResource::Kind::Texture2D:
    return D3D_SRV_DIMENSION_TEXTURE2D;
  case DxilResource::Kind::Texture2DArray:
    return D3D_SRV_DIMENSION_TEXTURE2DARRAY;
  case DxilResource::Kind::Texture2DMS:
    return D3D_SRV_DIMENSION_TEXTURE2DMS;
  case DxilResource::Kind::Texture2DMSArray:
    return D3D_SRV_DIMENSION_TEXTURE2DMSARRAY;
  case DxilResource::Kind::Texture3D:
    return D3D_SRV_DIMENSION_TEXTURE3D;
  case DxilResource::Kind::TextureCube:
    return D3D_SRV_DIMENSION_TEXTURECUBE;
  case DxilResource::Kind::TextureCubeArray:
    return D3D_SRV_DIMENSION_TEXTURECUBEARRAY;
  case DxilResource::Kind::RawBuffer:
    return D3D11_SRV_DIMENSION_BUFFER; // D3D11_SRV_DIMENSION_BUFFEREX?
  default:
    return D3D_SRV_DIMENSION_UNKNOWN;
  }
}

static UINT ResourceToFlags(DxilResourceBase *RB) {
  UINT result = 0;
  DxilResource *R = DxilResourceFromBase(RB);
  if (R != nullptr &&
      (R->IsAnyTexture() || R->GetKind() == DXIL::ResourceKind::TypedBuffer)) {
    llvm::Type *RetTy = R->GetRetType();
    if (VectorType *VT = dyn_cast<VectorType>(RetTy)) {
      unsigned vecSize = VT->getNumElements();
      switch (vecSize) {
      case 4:
        result |= D3D_SIF_TEXTURE_COMPONENTS;
        break;
      case 3:
        result |= D3D_SIF_TEXTURE_COMPONENT_1;
        break;
      case 2:
        result |= D3D_SIF_TEXTURE_COMPONENT_0;
        break;
      }
    }
  }
  // D3D_SIF_USERPACKED
  if (RB->GetClass() == DXIL::ResourceClass::Sampler) {
    DxilSampler *S = static_cast<DxilSampler *>(RB);
    if (S->GetSamplerKind() == DXIL::SamplerKind::Comparison)
      result |= D3D_SIF_COMPARISON_SAMPLER;
  }
  return result;
}

void DxilModuleReflection::CreateReflectionObjectForResource(DxilResourceBase *RB) {
  DxilResourceBase::Class C = RB->GetClass();
  DxilResource *R =
      (C == DXIL::ResourceClass::UAV || C == DXIL::ResourceClass::SRV)
          ? (DxilResource *)RB
          : nullptr;
  D3D12_SHADER_INPUT_BIND_DESC inputBind;
  ZeroMemory(&inputBind, sizeof(inputBind));
  inputBind.BindCount = RB->GetRangeSize();
  if (RB->GetRangeSize() == UINT_MAX)
    inputBind.BindCount = 0;
  inputBind.BindPoint = RB->GetLowerBound();
  inputBind.Dimension = ResourceToDimension(RB);
  inputBind.Name = RB->GetGlobalName().c_str();
  inputBind.Type = ResourceToShaderInputType(RB);
  if (R == nullptr) {
    inputBind.NumSamples = 0;
  }
  else {
    inputBind.NumSamples = R->GetSampleCount();
    if (inputBind.NumSamples == 0) {
      if (R->IsStructuredBuffer()) {
        inputBind.NumSamples = CalcResTypeSize(*m_pDxilModule, *R);
      }
      else if (!R->IsRawBuffer()) {
        inputBind.NumSamples = 0xFFFFFFFF;
      }
    }
  }
  inputBind.ReturnType = ResourceToReturnType(RB);
  inputBind.Space = RB->GetSpaceID();
  inputBind.uFlags = ResourceToFlags(RB);
  inputBind.uID = RB->GetID();
  m_Resources.push_back(inputBind);
}

// Find the imm offset part from a value.
// It must exist unless offset is 0.
static unsigned GetCBOffset(Value *V) {
  if (ConstantInt *Imm = dyn_cast<ConstantInt>(V))
    return Imm->getLimitedValue();
  else if (UnaryInstruction *UI = dyn_cast<UnaryInstruction>(V)) {
    return 0;
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(V)) {
    switch (BO->getOpcode()) {
    case Instruction::Add: {
      unsigned left = GetCBOffset(BO->getOperand(0));
      unsigned right = GetCBOffset(BO->getOperand(1));
      return left + right;
    } break;
    case Instruction::Or: {
      unsigned left = GetCBOffset(BO->getOperand(0));
      unsigned right = GetCBOffset(BO->getOperand(1));
      return left | right;
    } break;
    default:
      return 0;
    }
  } else {
    return 0;
  }
}

void CollectInPhiChain(PHINode *cbUser, std::vector<unsigned> &cbufUsage,
                  unsigned offset, std::unordered_set<Value *> &userSet) {
  if (userSet.count(cbUser) > 0)
    return;

  userSet.insert(cbUser);
  for (User *cbU : cbUser->users()) {
    if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(cbU)) {
      for (unsigned idx : EV->getIndices()) {
        cbufUsage.emplace_back(offset + idx * 4);
      }
    } else {
      PHINode *phi = cast<PHINode>(cbU);
      CollectInPhiChain(phi, cbufUsage, offset, userSet);
    }
  }
}

static void CollectCBufUsage(Value *cbHandle,
                             std::vector<unsigned> &cbufUsage) {
  for (User *U : cbHandle->users()) {
    CallInst *CI = cast<CallInst>(U);
    ConstantInt *opcodeV =
        cast<ConstantInt>(CI->getArgOperand(DXIL::OperandIndex::kOpcodeIdx));
    DXIL::OpCode opcode = static_cast<DXIL::OpCode>(opcodeV->getLimitedValue());
    if (opcode == DXIL::OpCode::CBufferLoadLegacy) {
      DxilInst_CBufferLoadLegacy cbload(CI);
      Value *resIndex = cbload.get_regIndex();
      unsigned offset = GetCBOffset(resIndex);
      // 16 bytes align.
      offset <<= 4;
      for (User *cbU : U->users()) {
        if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(cbU)) {
          for (unsigned idx : EV->getIndices()) {
            cbufUsage.emplace_back(offset + idx * 4);
          }
        } else {
          PHINode *phi = cast<PHINode>(cbU);
          std::unordered_set<Value *> userSet;
          CollectInPhiChain(phi, cbufUsage, offset, userSet);
        }
      }
    } else if (opcode == DXIL::OpCode::CBufferLoad) {
      DxilInst_CBufferLoad cbload(CI);
      Value *byteOffset = cbload.get_byteOffset();
      unsigned offset = GetCBOffset(byteOffset);
      cbufUsage.emplace_back(offset);
    } else {
      //
      DXASSERT(0, "invalid opcode");
    }
  }
}

static void SetCBufVarUsage(CShaderReflectionConstantBuffer &cb,
                            std::vector<unsigned> usage) {
  D3D12_SHADER_BUFFER_DESC Desc;
  if (FAILED(cb.GetDesc(&Desc)))
    return;

  unsigned size = Desc.Variables;

  std::sort(usage.begin(), usage.end());
  for (unsigned i = 0; i < size; i++) {
    ID3D12ShaderReflectionVariable *pVar = cb.GetVariableByIndex(i);
    D3D12_SHADER_VARIABLE_DESC VarDesc;
    if (FAILED(pVar->GetDesc(&VarDesc)))
      continue;
    if (!pVar)
      continue;

    unsigned begin = VarDesc.StartOffset;
    unsigned end = begin + VarDesc.Size;
    auto beginIt = std::find_if(usage.begin(), usage.end(),
                                [&](unsigned v) { return v >= begin; });
    auto endIt = std::find_if(usage.begin(), usage.end(),
                              [&](unsigned v) { return v >= end; });

    bool used = beginIt != endIt;
    // Clear used.
    if (!used) {
      CShaderReflectionType *pVarType = (CShaderReflectionType *)pVar->GetType();
      BYTE *pDefaultValue = nullptr;

      VarDesc.uFlags &= ~D3D_SVF_USED;
      CShaderReflectionVariable *pCVarDesc = (CShaderReflectionVariable*)pVar;
      pCVarDesc->Initialize(&cb, &VarDesc, pVarType, pDefaultValue);
    }
  }
}

void DxilShaderReflection::SetCBufferUsage() {
  hlsl::OP *hlslOP = m_pDxilModule->GetOP();
  LLVMContext &Ctx = m_pDxilModule->GetCtx();

  // Indexes >= cbuffer size from DxilModule are SRV or UAV structured buffers.
  // We only collect usage for actual cbuffers, so don't go clearing usage on other buffers.
  unsigned cbSize = std::min(m_CBs.size(), m_pDxilModule->GetCBuffers().size());
  std::vector< std::vector<unsigned> > cbufUsage(cbSize);

  Function *createHandle = hlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));

  if (createHandle->user_empty()) {
    createHandle->eraseFromParent();
    return;
  }

  // Find all cb handles.
  for (User *U : createHandle->users()) {
    DxilInst_CreateHandle handle(cast<CallInst>(U));
    Value *resClass = handle.get_resourceClass();
    ConstantInt *immResClass = cast<ConstantInt>(resClass);
    if (immResClass->getLimitedValue() == (unsigned)DXIL::ResourceClass::CBuffer) {
      ConstantInt *cbID = cast<ConstantInt>(handle.get_rangeId());
      CollectCBufUsage(U, cbufUsage[cbID->getLimitedValue()]);
    }
  }

  for (unsigned i=0;i<cbSize;i++) {
    SetCBufVarUsage(*m_CBs[i], cbufUsage[i]);
  }
}

void DxilModuleReflection::CreateReflectionObjects() {
  DXASSERT_NOMSG(m_pDxilModule != nullptr);

  // Create constant buffers, resources and signatures.
  for (auto && cb : m_pDxilModule->GetCBuffers()) {
    std::unique_ptr<CShaderReflectionConstantBuffer> rcb(new CShaderReflectionConstantBuffer());
    rcb->Initialize(*m_pDxilModule, *(cb.get()), m_Types);
    m_CBs.emplace_back(std::move(rcb));
  }

  // TODO: add tbuffers into m_CBs
  for (auto && uav : m_pDxilModule->GetUAVs()) {
    if (uav->GetKind() != DxilResource::Kind::StructuredBuffer) {
      continue;
    }
    std::unique_ptr<CShaderReflectionConstantBuffer> rcb(new CShaderReflectionConstantBuffer());
    rcb->InitializeStructuredBuffer(*m_pDxilModule, *(uav.get()), m_Types);
    m_CBs.emplace_back(std::move(rcb));
  }
  for (auto && srv : m_pDxilModule->GetSRVs()) {
    if (srv->GetKind() != DxilResource::Kind::StructuredBuffer) {
      continue;
    }
    std::unique_ptr<CShaderReflectionConstantBuffer> rcb(new CShaderReflectionConstantBuffer());
    rcb->InitializeStructuredBuffer(*m_pDxilModule, *(srv.get()), m_Types);
    m_CBs.emplace_back(std::move(rcb));
  }

  // Populate all resources.
  for (auto && cbRes : m_pDxilModule->GetCBuffers()) {
    CreateReflectionObjectForResource(cbRes.get());
  }
  for (auto && samplerRes : m_pDxilModule->GetSamplers()) {
    CreateReflectionObjectForResource(samplerRes.get());
  }
  for (auto && srvRes : m_pDxilModule->GetSRVs()) {
    CreateReflectionObjectForResource(srvRes.get());
  }
  for (auto && uavRes : m_pDxilModule->GetUAVs()) {
    CreateReflectionObjectForResource(uavRes.get());
  }
}

static D3D_REGISTER_COMPONENT_TYPE CompTypeToRegisterComponentType(CompType CT) {
  switch (CT.GetKind()) {
  case DXIL::ComponentType::F16:
  case DXIL::ComponentType::F32:
    return D3D_REGISTER_COMPONENT_FLOAT32;
  case DXIL::ComponentType::I1:
  case DXIL::ComponentType::U16:
  case DXIL::ComponentType::U32:
    return D3D_REGISTER_COMPONENT_UINT32;
  case DXIL::ComponentType::I16:
  case DXIL::ComponentType::I32:
    return D3D_REGISTER_COMPONENT_SINT32;
  default:
    return D3D_REGISTER_COMPONENT_UNKNOWN;
  }
}

static D3D_MIN_PRECISION CompTypeToMinPrecision(CompType CT) {
  switch (CT.GetKind()) {
  case DXIL::ComponentType::F16:
    return D3D_MIN_PRECISION_FLOAT_16;
  case DXIL::ComponentType::I16:
    return D3D_MIN_PRECISION_SINT_16;
  case DXIL::ComponentType::U16:
    return D3D_MIN_PRECISION_UINT_16;
  default:
    return D3D_MIN_PRECISION_DEFAULT;
  }
}

D3D_NAME SemanticToSystemValueType(const Semantic *S, DXIL::TessellatorDomain domain) {
  switch (S->GetKind()) {
  case Semantic::Kind::ClipDistance:
    return D3D_NAME_CLIP_DISTANCE;
  case Semantic::Kind::Arbitrary:
    return D3D_NAME_UNDEFINED;
  case Semantic::Kind::VertexID:
    return D3D_NAME_VERTEX_ID;
  case Semantic::Kind::InstanceID:
    return D3D_NAME_INSTANCE_ID;
  case Semantic::Kind::Position:
    return D3D_NAME_POSITION;
  case Semantic::Kind::Coverage:
    return D3D_NAME_COVERAGE;
  case Semantic::Kind::InnerCoverage:
    return D3D_NAME_INNER_COVERAGE;
  case Semantic::Kind::PrimitiveID:
    return D3D_NAME_PRIMITIVE_ID;
  case Semantic::Kind::SampleIndex:
    return D3D_NAME_SAMPLE_INDEX;
  case Semantic::Kind::IsFrontFace:
    return D3D_NAME_IS_FRONT_FACE;
  case Semantic::Kind::RenderTargetArrayIndex:
    return D3D_NAME_RENDER_TARGET_ARRAY_INDEX;
  case Semantic::Kind::ViewPortArrayIndex:
    return D3D_NAME_VIEWPORT_ARRAY_INDEX;
  case Semantic::Kind::CullDistance:
    return D3D_NAME_CULL_DISTANCE;
  case Semantic::Kind::Target:
    return D3D_NAME_TARGET;
  case Semantic::Kind::Depth:
    return D3D_NAME_DEPTH;
  case Semantic::Kind::DepthLessEqual:
    return D3D_NAME_DEPTH_LESS_EQUAL;
  case Semantic::Kind::DepthGreaterEqual:
    return D3D_NAME_DEPTH_GREATER_EQUAL;
  case Semantic::Kind::StencilRef:
    return D3D_NAME_STENCIL_REF;
  case Semantic::Kind::TessFactor: {
    switch (domain) {
    case DXIL::TessellatorDomain::IsoLine:
        return D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR;
    case DXIL::TessellatorDomain::Tri:
        return D3D_NAME_FINAL_TRI_EDGE_TESSFACTOR;
    case DXIL::TessellatorDomain::Quad:
        return D3D_NAME_FINAL_QUAD_EDGE_TESSFACTOR;
    default:
    return D3D_NAME_UNDEFINED;
    }
  }
  case Semantic::Kind::InsideTessFactor:
    switch (domain) {
    case DXIL::TessellatorDomain::Tri:
        return D3D_NAME_FINAL_TRI_INSIDE_TESSFACTOR;
    case DXIL::TessellatorDomain::Quad:
        return D3D_NAME_FINAL_QUAD_INSIDE_TESSFACTOR;
    default:
    return D3D_NAME_UNDEFINED;
    }
  case Semantic::Kind::DispatchThreadID:
  case Semantic::Kind::GroupID:
  case Semantic::Kind::GroupIndex:
  case Semantic::Kind::GroupThreadID:
  case Semantic::Kind::DomainLocation:
  case Semantic::Kind::OutputControlPointID:
  case Semantic::Kind::GSInstanceID:
  case Semantic::Kind::Invalid:
  default:
    return D3D_NAME_UNDEFINED;
  }
}

static uint8_t NegMask(uint8_t V) {
  V ^= 0xF;
  return V & 0xF;
}

void DxilShaderReflection::CreateReflectionObjectsForSignature(
  const DxilSignature &Sig,
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC> &Descs) {
  bool clipDistanceSeen = false;
  for (auto && SigElem : Sig.GetElements()) {
    D3D12_SIGNATURE_PARAMETER_DESC Desc;

    // TODO: why do we have multiple SV_ClipDistance elements?
    if (SigElem->GetSemantic()->GetKind() == DXIL::SemanticKind::ClipDistance) {
      if (clipDistanceSeen) continue;
      clipDistanceSeen = true;
    }

    Desc.ComponentType = CompTypeToRegisterComponentType(SigElem->GetCompType());
    Desc.Mask = SigElem->GetColsAsMask();
    // D3D11_43 does not have MinPrecison.
    if (m_PublicAPI != PublicAPI::D3D11_43)
      Desc.MinPrecision = CompTypeToMinPrecision(SigElem->GetCompType());
    Desc.ReadWriteMask = Sig.IsInput() ? 0 : Desc.Mask; // Start with output-never-written/input-never-read.
    Desc.Register = SigElem->GetStartRow();
    Desc.Stream = SigElem->GetOutputStream();
    Desc.SystemValueType = SemanticToSystemValueType(SigElem->GetSemantic(), m_pDxilModule->GetTessellatorDomain());
    Desc.SemanticName = SigElem->GetName();
    if (!SigElem->GetSemantic()->IsArbitrary())
      Desc.SemanticName = CreateUpperCase(Desc.SemanticName);

    const std::vector<unsigned> &indexVec = SigElem->GetSemanticIndexVec();
    for (unsigned semIdx = 0; semIdx < indexVec.size(); ++semIdx) {
      Desc.SemanticIndex = indexVec[semIdx];
      if (Desc.SystemValueType == D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR &&
          Desc.SemanticIndex == 1)
        Desc.SystemValueType = D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR;
      Descs.push_back(Desc);
    }
  }
}

LPCSTR DxilShaderReflection::CreateUpperCase(LPCSTR pValue) {
  // Restricted only to [a-z] ASCII.
  LPCSTR pCursor = pValue;
  while (*pCursor != '\0') {
    if ('a' <= *pCursor && *pCursor <= 'z') {
      break;
    }
    ++pCursor;
  }
  if (*pCursor == '\0')
    return pValue;

  std::unique_ptr<char[]> pUpperStr = std::make_unique<char[]>(strlen(pValue) + 1);
  char *pWrite = pUpperStr.get();
  pCursor = pValue;
  for (;;) {
    *pWrite = *pCursor;
    if ('a' <= *pWrite && *pWrite <= 'z') {
      *pWrite += ('A' - 'a');
    }
    if (*pWrite == '\0') break;
    ++pWrite;
    ++pCursor;
  }
  m_UpperCaseNames.push_back(std::move(pUpperStr));
  return m_UpperCaseNames.back().get();
}

HRESULT DxilModuleReflection::LoadModule(IDxcBlob *pBlob,
                                         const DxilPartHeader *pPart) {
  DXASSERT_NOMSG(pBlob != nullptr);
  DXASSERT_NOMSG(pPart != nullptr);
  m_pContainer = pBlob;
  const char *pData = GetDxilPartData(pPart);
  try {
    const char *pBitcode;
    uint32_t bitcodeLength;
    GetDxilProgramBitcode((DxilProgramHeader *)pData, &pBitcode, &bitcodeLength);
    std::unique_ptr<MemoryBuffer> pMemBuffer =
        MemoryBuffer::getMemBufferCopy(StringRef(pBitcode, bitcodeLength));
#if 0 // We materialize eagerly, because we'll need to walk instructions to look for usage information.
    ErrorOr<std::unique_ptr<Module>> module =
        getLazyBitcodeModule(std::move(pMemBuffer), Context);
#else
    ErrorOr<std::unique_ptr<Module>> module =
      parseBitcodeFile(pMemBuffer->getMemBufferRef(), Context, nullptr);
#endif
    if (!module) {
      return E_INVALIDARG;
    }
    std::swap(m_pModule, module.get());
    m_pDxilModule = &m_pModule->GetOrCreateDxilModule();
    CreateReflectionObjects();
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
};

HRESULT DxilShaderReflection::Load(IDxcBlob *pBlob,
                                   const DxilPartHeader *pPart) {
  IFR(LoadModule(pBlob, pPart));

  try {
    // Set cbuf usage.
    SetCBufferUsage();

    // Populate input/output/patch constant signatures.
    CreateReflectionObjectsForSignature(m_pDxilModule->GetInputSignature(), m_InputSignature);
    CreateReflectionObjectsForSignature(m_pDxilModule->GetOutputSignature(), m_OutputSignature);
    CreateReflectionObjectsForSignature(m_pDxilModule->GetPatchConstantSignature(), m_PatchConstantSignature);
    MarkUsedSignatureElements();
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetDesc(D3D12_SHADER_DESC *pDesc) {
  IFR(ZeroMemoryToOut(pDesc));
  const DxilModule &M = *m_pDxilModule;
  const ShaderModel *pSM = M.GetShaderModel();

  pDesc->Version = EncodeVersion(pSM->GetKind(), pSM->GetMajor(), pSM->GetMinor());
  // Unset:  LPCSTR                  Creator;                     // Creator string
  // Unset:  UINT                    Flags;                       // Shader compilation/parse flags

  pDesc->ConstantBuffers = m_CBs.size();
  pDesc->BoundResources = m_Resources.size();
  pDesc->InputParameters = m_InputSignature.size();
  pDesc->OutputParameters = m_OutputSignature.size();
  pDesc->PatchConstantParameters = m_PatchConstantSignature.size();

  // Unset:  UINT                    InstructionCount;            // Number of emitted instructions
  // Unset:  UINT                    TempRegisterCount;           // Number of temporary registers used 
  // Unset:  UINT                    TempArrayCount;              // Number of temporary arrays used
  // Unset:  UINT                    DefCount;                    // Number of constant defines 
  // Unset:  UINT                    DclCount;                    // Number of declarations (input + output)
  // Unset:  UINT                    TextureNormalInstructions;   // Number of non-categorized texture instructions
  // Unset:  UINT                    TextureLoadInstructions;     // Number of texture load instructions
  // Unset:  UINT                    TextureCompInstructions;     // Number of texture comparison instructions
  // Unset:  UINT                    TextureBiasInstructions;     // Number of texture bias instructions
  // Unset:  UINT                    TextureGradientInstructions; // Number of texture gradient instructions
  // Unset:  UINT                    FloatInstructionCount;       // Number of floating point arithmetic instructions used
  // Unset:  UINT                    IntInstructionCount;         // Number of signed integer arithmetic instructions used
  // Unset:  UINT                    UintInstructionCount;        // Number of unsigned integer arithmetic instructions used
  // Unset:  UINT                    StaticFlowControlCount;      // Number of static flow control instructions used
  // Unset:  UINT                    DynamicFlowControlCount;     // Number of dynamic flow control instructions used
  // Unset:  UINT                    MacroInstructionCount;       // Number of macro instructions used
  // Unset:  UINT                    ArrayInstructionCount;       // Number of array instructions used
  // Unset:  UINT                    CutInstructionCount;         // Number of cut instructions used
  // Unset:  UINT                    EmitInstructionCount;        // Number of emit instructions used
  // Unset:  D3D_PRIMITIVE_TOPOLOGY  GSOutputTopology;            // Geometry shader output topology
  // Unset:  UINT                    GSMaxOutputVertexCount;      // Geometry shader maximum output vertex count
  // Unset:  D3D_PRIMITIVE           InputPrimitive;              // GS/HS input primitive
  // Unset:  UINT                    cGSInstanceCount;            // Number of Geometry shader instances
  // Unset:  UINT                    cControlPoints;              // Number of control points in the HS->DS stage
  // Unset:  D3D_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive;  // Primitive output by the tessellator
  // Unset:  D3D_TESSELLATOR_PARTITIONING HSPartitioning;         // Partitioning mode of the tessellator
  // Unset:  D3D_TESSELLATOR_DOMAIN  TessellatorDomain;           // Domain of the tessellator (quad, tri, isoline)
  // instruction counts
  // Unset:  UINT cBarrierInstructions;                           // Number of barrier instructions in a compute shader
  // Unset:  UINT cInterlockedInstructions;                       // Number of interlocked instructions
  // Unset:  UINT cTextureStoreInstructions;                      // Number of texture writes
  return S_OK;
}

static bool GetUnsignedVal(Value *V, uint32_t *pValue) {
  ConstantInt *CI = dyn_cast<ConstantInt>(V);
  if (!CI) return false;
  uint64_t u = CI->getZExtValue();
  if (u > UINT32_MAX) return false;
  *pValue = (uint32_t)u;
  return true;
}

void DxilShaderReflection::MarkUsedSignatureElements() {
  Function *F = m_pDxilModule->GetEntryFunction();
  DXASSERT(F != nullptr, "else module load should have failed");
  // For every loadInput/storeOutput, update the corresponding ReadWriteMask.
  // F is a pointer to a Function instance
  unsigned elementCount = m_InputSignature.size() + m_OutputSignature.size() +
                          m_PatchConstantSignature.size();
  unsigned markedElementCount = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    DxilInst_LoadInput LI(&*I);
    DxilInst_StoreOutput SO(&*I);
    DxilInst_LoadPatchConstant LPC(&*I);
    DxilInst_StorePatchConstant SPC(&*I);
    std::vector<D3D12_SIGNATURE_PARAMETER_DESC> *pDescs;
    const DxilSignature *pSig;
    uint32_t col, row, sigId;
    if (LI) {
      if (!GetUnsignedVal(LI.get_inputSigId(), &sigId)) continue;
      if (!GetUnsignedVal(LI.get_colIndex(), &col)) continue;
      if (!GetUnsignedVal(LI.get_rowIndex(), &row)) continue;
      pDescs = &m_InputSignature;
      pSig = &m_pDxilModule->GetInputSignature();
    }
    else if (SO) {
      if (!GetUnsignedVal(SO.get_outputSigId(), &sigId)) continue;
      if (!GetUnsignedVal(SO.get_colIndex(), &col)) continue;
      if (!GetUnsignedVal(SO.get_rowIndex(), &row)) continue;
      pDescs = &m_OutputSignature;
      pSig = &m_pDxilModule->GetOutputSignature();
    }
    else if (SPC) {
      if (!GetUnsignedVal(SPC.get_outputSigID(), &sigId)) continue;
      if (!GetUnsignedVal(SPC.get_col(), &col)) continue;
      if (!GetUnsignedVal(SPC.get_row(), &row)) continue;
      pDescs = &m_PatchConstantSignature;
      pSig = &m_pDxilModule->GetPatchConstantSignature();
    }
    else if (LPC) {
      if (!GetUnsignedVal(LPC.get_inputSigId(), &sigId)) continue;
      if (!GetUnsignedVal(LPC.get_col(), &col)) continue;
      if (!GetUnsignedVal(LPC.get_row(), &row)) continue;
      pDescs = &m_PatchConstantSignature;
      pSig = &m_pDxilModule->GetPatchConstantSignature();
    }
    else {
      continue;
    }

    if (sigId >= pDescs->size()) continue;

    D3D12_SIGNATURE_PARAMETER_DESC *pDesc = &(*pDescs)[sigId];
    // Consider being more fine-grained about masks.
    // We report sometimes-read on input as always-read.
    unsigned UsedMask = pSig->IsInput() ? pDesc->Mask : NegMask(pDesc->Mask);
    if (pDesc->ReadWriteMask == UsedMask)
      continue;
    pDesc->ReadWriteMask = UsedMask;
    ++markedElementCount;
    if (markedElementCount == elementCount)
      return;
  }
}

_Use_decl_annotations_
ID3D12ShaderReflectionConstantBuffer* DxilShaderReflection::GetConstantBufferByIndex(UINT Index) {
  return DxilModuleReflection::_GetConstantBufferByIndex(Index);
}
ID3D12ShaderReflectionConstantBuffer* DxilModuleReflection::_GetConstantBufferByIndex(UINT Index) {
  if (Index >= m_CBs.size()) {
    return &g_InvalidSRConstantBuffer;
  }
  return m_CBs[Index].get();
}

_Use_decl_annotations_
ID3D12ShaderReflectionConstantBuffer* DxilShaderReflection::GetConstantBufferByName(LPCSTR Name) {
  return DxilModuleReflection::_GetConstantBufferByName(Name);
}
ID3D12ShaderReflectionConstantBuffer* DxilModuleReflection::_GetConstantBufferByName(LPCSTR Name) {
  if (!Name) {
    return &g_InvalidSRConstantBuffer;
  }
  for (UINT index = 0; index < m_CBs.size(); ++index) {
    if (0 == strcmp(m_CBs[index]->GetName(), Name)) {
      return m_CBs[index].get();
    }
  }
  return &g_InvalidSRConstantBuffer;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetResourceBindingDesc(UINT ResourceIndex,
  _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc) {
  return DxilModuleReflection::_GetResourceBindingDesc(ResourceIndex, pDesc, m_PublicAPI);
}
HRESULT DxilModuleReflection::_GetResourceBindingDesc(UINT ResourceIndex,
  _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc, PublicAPI api) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ResourceIndex < m_Resources.size(), E_INVALIDARG);
  if (api != PublicAPI::D3D12) {
    memcpy(pDesc, &m_Resources[ResourceIndex], sizeof(D3D11_SHADER_INPUT_BIND_DESC));
  }
  else {
    *pDesc = m_Resources[ResourceIndex];
  }
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetInputParameterDesc(UINT ParameterIndex,
  _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ParameterIndex < m_InputSignature.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D11_43)
    *pDesc = m_InputSignature[ParameterIndex];
  else
    memcpy(pDesc, &m_InputSignature[ParameterIndex],
           // D3D11_43 does not have MinPrecison.
           sizeof(D3D12_SIGNATURE_PARAMETER_DESC) - sizeof(D3D_MIN_PRECISION));

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetOutputParameterDesc(UINT ParameterIndex,
  D3D12_SIGNATURE_PARAMETER_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ParameterIndex < m_OutputSignature.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D11_43)
    *pDesc = m_OutputSignature[ParameterIndex];
  else
    memcpy(pDesc, &m_OutputSignature[ParameterIndex],
           // D3D11_43 does not have MinPrecison.
           sizeof(D3D12_SIGNATURE_PARAMETER_DESC) - sizeof(D3D_MIN_PRECISION));

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetPatchConstantParameterDesc(UINT ParameterIndex,
  D3D12_SIGNATURE_PARAMETER_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ParameterIndex < m_PatchConstantSignature.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D11_43)
    *pDesc = m_PatchConstantSignature[ParameterIndex];
  else
    memcpy(pDesc, &m_PatchConstantSignature[ParameterIndex],
           // D3D11_43 does not have MinPrecison.
           sizeof(D3D12_SIGNATURE_PARAMETER_DESC) - sizeof(D3D_MIN_PRECISION));

  return S_OK;
}

_Use_decl_annotations_
ID3D12ShaderReflectionVariable* DxilShaderReflection::GetVariableByName(LPCSTR Name) {
  return DxilModuleReflection::_GetVariableByName(Name);
}
ID3D12ShaderReflectionVariable* DxilModuleReflection::_GetVariableByName(LPCSTR Name) {
  if (Name != nullptr) {
    // Iterate through all cbuffers to find the variable.
    for (UINT i = 0; i < m_CBs.size(); i++) {
      ID3D12ShaderReflectionVariable *pVar = m_CBs[i]->GetVariableByName(Name);
      if (pVar != &g_InvalidSRVariable) {
        return pVar;
      }
    }
  }

  return &g_InvalidSRVariable;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetResourceBindingDescByName(LPCSTR Name,
  D3D12_SHADER_INPUT_BIND_DESC *pDesc) {
  return DxilModuleReflection::_GetResourceBindingDescByName(Name, pDesc, m_PublicAPI);
}
HRESULT DxilModuleReflection::_GetResourceBindingDescByName(LPCSTR Name,
  D3D12_SHADER_INPUT_BIND_DESC *pDesc, PublicAPI api) {
  IFRBOOL(Name != nullptr, E_INVALIDARG);

  for (UINT i = 0; i < m_Resources.size(); i++) {
    if (strcmp(m_Resources[i].Name, Name) == 0) {
      if (api != PublicAPI::D3D12) {
        memcpy(pDesc, &m_Resources[i], sizeof(D3D11_SHADER_INPUT_BIND_DESC));
      }
      else {
        *pDesc = m_Resources[i];
      }
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

UINT DxilShaderReflection::GetMovInstructionCount() { return 0; }
UINT DxilShaderReflection::GetMovcInstructionCount() { return 0; }
UINT DxilShaderReflection::GetConversionInstructionCount() { return 0; }
UINT DxilShaderReflection::GetBitwiseInstructionCount() { return 0; }

D3D_PRIMITIVE DxilShaderReflection::GetGSInputPrimitive() {
  if (!m_pDxilModule->GetShaderModel()->IsGS())
    return D3D_PRIMITIVE::D3D10_PRIMITIVE_UNDEFINED;
  return (D3D_PRIMITIVE)m_pDxilModule->GetInputPrimitive();
}

BOOL DxilShaderReflection::IsSampleFrequencyShader() {
  // TODO: determine correct value
  return FALSE;
}

UINT DxilShaderReflection::GetNumInterfaceSlots() { return 0; }

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetMinFeatureLevel(enum D3D_FEATURE_LEVEL* pLevel) {
  IFR(AssignToOut(D3D_FEATURE_LEVEL_12_0, pLevel));
  return S_OK;
}

_Use_decl_annotations_
UINT DxilShaderReflection::GetThreadGroupSize(UINT *pSizeX, UINT *pSizeY, UINT *pSizeZ) {
  if (!m_pDxilModule->GetShaderModel()->IsCS()) {
    AssignToOutOpt((UINT)0, pSizeX);
    AssignToOutOpt((UINT)0, pSizeY);
    AssignToOutOpt((UINT)0, pSizeZ);
    return 0;
  }
  unsigned x = m_pDxilModule->GetNumThreads(0);
  unsigned y = m_pDxilModule->GetNumThreads(1);
  unsigned z = m_pDxilModule->GetNumThreads(2);
  AssignToOutOpt(x, pSizeX);
  AssignToOutOpt(y, pSizeY);
  AssignToOutOpt(z, pSizeZ);
  return x * y * z;
}

UINT64 DxilShaderReflection::GetRequiresFlags() {
  UINT64 result = 0;
  uint64_t features = m_pDxilModule->m_ShaderFlags.GetFeatureInfo();
  if (features & ShaderFeatureInfo_Doubles) result |= D3D_SHADER_REQUIRES_DOUBLES;
  if (features & ShaderFeatureInfo_UAVsAtEveryStage) result |= D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE;
  if (features & ShaderFeatureInfo_64UAVs) result |= D3D_SHADER_REQUIRES_64_UAVS;
  if (features & ShaderFeatureInfo_MinimumPrecision) result |= D3D_SHADER_REQUIRES_MINIMUM_PRECISION;
  if (features & ShaderFeatureInfo_11_1_DoubleExtensions) result |= D3D_SHADER_REQUIRES_11_1_DOUBLE_EXTENSIONS;
  if (features & ShaderFeatureInfo_11_1_ShaderExtensions) result |= D3D_SHADER_REQUIRES_11_1_SHADER_EXTENSIONS;
  if (features & ShaderFeatureInfo_LEVEL9ComparisonFiltering) result |= D3D_SHADER_REQUIRES_LEVEL_9_COMPARISON_FILTERING;
  if (features & ShaderFeatureInfo_TiledResources) result |= D3D_SHADER_REQUIRES_TILED_RESOURCES;
  if (features & ShaderFeatureInfo_StencilRef) result |= D3D_SHADER_REQUIRES_STENCIL_REF;
  if (features & ShaderFeatureInfo_InnerCoverage) result |= D3D_SHADER_REQUIRES_INNER_COVERAGE;
  if (features & ShaderFeatureInfo_TypedUAVLoadAdditionalFormats) result |= D3D_SHADER_REQUIRES_TYPED_UAV_LOAD_ADDITIONAL_FORMATS;
  if (features & ShaderFeatureInfo_ROVs) result |= D3D_SHADER_REQUIRES_ROVS;
  if (features & ShaderFeatureInfo_ViewportAndRTArrayIndexFromAnyShaderFeedingRasterizer) result |= D3D_SHADER_REQUIRES_VIEWPORT_AND_RT_ARRAY_INDEX_FROM_ANY_SHADER_FEEDING_RASTERIZER;
  return result;
}


// ID3D12FunctionReflection

class CFunctionReflection : public ID3D12FunctionReflection {
protected:
  DxilLibraryReflection * m_pLibraryReflection = nullptr;
  const Function *m_pFunction;
  const DxilFunctionProps *m_pProps;  // nullptr if non-shader library function or patch constant function
  std::string m_Name;
  typedef SmallSetVector<UINT32, 8> ResourceUseSet;
  ResourceUseSet m_UsedResources;
  ResourceUseSet m_UsedCBs;

public:
  void Initialize(DxilLibraryReflection* pLibraryReflection, Function *pFunction) {
    DXASSERT_NOMSG(pLibraryReflection);
    DXASSERT_NOMSG(pFunction);
    m_pLibraryReflection = pLibraryReflection;
    m_pFunction = pFunction;

    const DxilModule &M = *m_pLibraryReflection->m_pDxilModule;
    m_Name = m_pFunction->getName().str();
    m_pProps = nullptr;
    if (M.HasDxilFunctionProps(m_pFunction)) {
      m_pProps = &M.GetDxilFunctionProps(m_pFunction);
    }
  }
  void AddResourceReference(UINT resIndex) {
    m_UsedResources.insert(resIndex);
  }
  void AddCBReference(UINT cbIndex) {
    m_UsedCBs.insert(cbIndex);
  }

  // ID3D12FunctionReflection
  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_FUNCTION_DESC * pDesc);

  // BufferIndex relative to used constant buffers here
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByIndex)(THIS_ _In_ UINT BufferIndex);
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByName)(THIS_ _In_ LPCSTR Name);

  STDMETHOD(GetResourceBindingDesc)(THIS_ _In_ UINT ResourceIndex,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC * pDesc);

  STDMETHOD_(ID3D12ShaderReflectionVariable *, GetVariableByName)(THIS_ _In_ LPCSTR Name);

  STDMETHOD(GetResourceBindingDescByName)(THIS_ _In_ LPCSTR Name,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC * pDesc);

  // Use D3D_RETURN_PARAMETER_INDEX to get description of the return value.
  STDMETHOD_(ID3D12FunctionParameterReflection *, GetFunctionParameter)(THIS_ _In_ INT ParameterIndex) {
    return &g_InvalidFunctionParameter;
  }
};

_Use_decl_annotations_
HRESULT CFunctionReflection::GetDesc(D3D12_FUNCTION_DESC *pDesc) {
  DXASSERT_NOMSG(m_pLibraryReflection);
  IFR(ZeroMemoryToOut(pDesc));

  const ShaderModel* pSM = m_pLibraryReflection->m_pDxilModule->GetShaderModel();
  DXIL::ShaderKind kind = DXIL::ShaderKind::Library;
  if (m_pProps) {
    kind = m_pProps->shaderKind;
  }
  pDesc->Version = EncodeVersion(kind, pSM->GetMajor(), pSM->GetMinor());

  //Unset:  LPCSTR                  Creator;                     // Creator string
  //Unset:  UINT                    Flags;                       // Shader compilation/parse flags

  pDesc->ConstantBuffers = (UINT)m_UsedCBs.size();
  pDesc->BoundResources = (UINT)m_UsedResources.size();

  //Unset:  UINT                    InstructionCount;            // Number of emitted instructions
  //Unset:  UINT                    TempRegisterCount;           // Number of temporary registers used 
  //Unset:  UINT                    TempArrayCount;              // Number of temporary arrays used
  //Unset:  UINT                    DefCount;                    // Number of constant defines 
  //Unset:  UINT                    DclCount;                    // Number of declarations (input + output)
  //Unset:  UINT                    TextureNormalInstructions;   // Number of non-categorized texture instructions
  //Unset:  UINT                    TextureLoadInstructions;     // Number of texture load instructions
  //Unset:  UINT                    TextureCompInstructions;     // Number of texture comparison instructions
  //Unset:  UINT                    TextureBiasInstructions;     // Number of texture bias instructions
  //Unset:  UINT                    TextureGradientInstructions; // Number of texture gradient instructions
  //Unset:  UINT                    FloatInstructionCount;       // Number of floating point arithmetic instructions used
  //Unset:  UINT                    IntInstructionCount;         // Number of signed integer arithmetic instructions used
  //Unset:  UINT                    UintInstructionCount;        // Number of unsigned integer arithmetic instructions used
  //Unset:  UINT                    StaticFlowControlCount;      // Number of static flow control instructions used
  //Unset:  UINT                    DynamicFlowControlCount;     // Number of dynamic flow control instructions used
  //Unset:  UINT                    MacroInstructionCount;       // Number of macro instructions used
  //Unset:  UINT                    ArrayInstructionCount;       // Number of array instructions used
  //Unset:  UINT                    MovInstructionCount;         // Number of mov instructions used
  //Unset:  UINT                    MovcInstructionCount;        // Number of movc instructions used
  //Unset:  UINT                    ConversionInstructionCount;  // Number of type conversion instructions used
  //Unset:  UINT                    BitwiseInstructionCount;     // Number of bitwise arithmetic instructions used
  //Unset:  D3D_FEATURE_LEVEL       MinFeatureLevel;             // Min target of the function byte code
  //Unset:  UINT64                  RequiredFeatureFlags;        // Required feature flags

  pDesc->Name = m_Name.c_str();

  //Unset:  INT                     FunctionParameterCount;      // Number of logical parameters in the function signature (not including return)
  //Unset:  BOOL                    HasReturn;                   // TRUE, if function returns a value, false - it is a subroutine
  //Unset:  BOOL                    Has10Level9VertexShader;     // TRUE, if there is a 10L9 VS blob
  //Unset:  BOOL                    Has10Level9PixelShader;      // TRUE, if there is a 10L9 PS blob
  return S_OK;
}

// BufferIndex is relative to used constant buffers here
ID3D12ShaderReflectionConstantBuffer *CFunctionReflection::GetConstantBufferByIndex(UINT BufferIndex) {
  DXASSERT_NOMSG(m_pLibraryReflection);
  if (BufferIndex >= m_UsedCBs.size())
    return &g_InvalidSRConstantBuffer;
  return m_pLibraryReflection->_GetConstantBufferByIndex(m_UsedCBs[BufferIndex]);
}

ID3D12ShaderReflectionConstantBuffer *CFunctionReflection::GetConstantBufferByName(LPCSTR Name) {
  DXASSERT_NOMSG(m_pLibraryReflection);
  return m_pLibraryReflection->_GetConstantBufferByName(Name);
}

HRESULT CFunctionReflection::GetResourceBindingDesc(UINT ResourceIndex,
  D3D12_SHADER_INPUT_BIND_DESC * pDesc) {
  DXASSERT_NOMSG(m_pLibraryReflection);
  if (ResourceIndex >= m_UsedResources.size())
    return E_INVALIDARG;
  return m_pLibraryReflection->_GetResourceBindingDesc(m_UsedResources[ResourceIndex], pDesc);
}

ID3D12ShaderReflectionVariable * CFunctionReflection::GetVariableByName(LPCSTR Name) {
  DXASSERT_NOMSG(m_pLibraryReflection);
  return m_pLibraryReflection->_GetVariableByName(Name);
}

HRESULT CFunctionReflection::GetResourceBindingDescByName(LPCSTR Name,
  D3D12_SHADER_INPUT_BIND_DESC * pDesc) {
  DXASSERT_NOMSG(m_pLibraryReflection);
  return m_pLibraryReflection->_GetResourceBindingDescByName(Name, pDesc);
}


// DxilLibraryReflection

// From DxilContainerAssembler:
static llvm::Function *FindUsingFunction(llvm::Value *User) {
  if (llvm::Instruction *I = dyn_cast<llvm::Instruction>(User)) {
    // Instruction should be inside a basic block, which is in a function
    return cast<llvm::Function>(I->getParent()->getParent());
  }
  // User can be either instruction, constant, or operator. But User is an
  // operator only if constant is a scalar value, not resource pointer.
  llvm::Constant *CU = cast<llvm::Constant>(User);
  if (!CU->user_empty())
    return FindUsingFunction(*CU->user_begin());
  else
    return nullptr;
}

void DxilLibraryReflection::AddResourceUseToFunctions(DxilResourceBase &resource, unsigned resIndex) {
  Constant *var = resource.GetGlobalSymbol();
  if (var) {
    for (auto user : var->users()) {
      // Find the function.
      if (llvm::Function *F = FindUsingFunction(user)) {
        auto funcReflector = m_FunctionsByPtr[F];
        funcReflector->AddResourceReference(resIndex);
        if (resource.GetClass() == DXIL::ResourceClass::CBuffer) {
          funcReflector->AddCBReference(resource.GetID());
        }
      }
    }
  }
}

void DxilLibraryReflection::AddResourceDependencies() {
  std::map<StringRef, CFunctionReflection*> orderedMap;
  for (auto &F : m_pModule->functions()) {
    if (F.isDeclaration())
      continue;
    auto &func = m_FunctionMap[F.getName()];
    DXASSERT(!func.get(), "otherwise duplicate named functions");
    func.reset(new CFunctionReflection());
    func->Initialize(this, &F);
    m_FunctionsByPtr[&F] = func.get();
    orderedMap[F.getName()] = func.get();
  }
  // Fill in function vector sorted by name
  m_FunctionVector.clear();
  m_FunctionVector.reserve(orderedMap.size());
  for (auto &it : orderedMap) {
    m_FunctionVector.push_back(it.second);
  }
  UINT resIndex = 0;
  for (auto &resource : m_Resources) {
    switch ((UINT32)resource.Type) {
    case D3D_SIT_CBUFFER:
      AddResourceUseToFunctions(m_pDxilModule->GetCBuffer(resource.uID), resIndex);
      break;
    case D3D_SIT_TBUFFER:   // TODO: Handle when TBuffers are added to CB list
    case D3D_SIT_TEXTURE:
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_BYTEADDRESS:
    case D3D_SIT_RTACCELERATIONSTRUCTURE:
      AddResourceUseToFunctions(m_pDxilModule->GetSRV(resource.uID), resIndex);
      break;
    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_APPEND_STRUCTURED:
    case D3D_SIT_UAV_CONSUME_STRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
      AddResourceUseToFunctions(m_pDxilModule->GetUAV(resource.uID), resIndex);
      break;
    case D3D_SIT_SAMPLER:
      AddResourceUseToFunctions(m_pDxilModule->GetSampler(resource.uID), resIndex);
      break;
    }
    resIndex++;
  }
}

// ID3D12LibraryReflection

HRESULT DxilLibraryReflection::Load(IDxcBlob *pBlob,
  const DxilPartHeader *pPart) {
  IFR(LoadModule(pBlob, pPart));

  try {
    AddResourceDependencies();
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}

_Use_decl_annotations_
HRESULT DxilLibraryReflection::GetDesc(D3D12_LIBRARY_DESC * pDesc) {
  IFR(ZeroMemoryToOut(pDesc));
  //Unset:  LPCSTR    Creator;           // The name of the originator of the library.
  //Unset:  UINT      Flags;             // Compilation flags.
  //UINT      FunctionCount;     // Number of functions exported from the library.
  pDesc->FunctionCount = (UINT)m_FunctionVector.size();
  return S_OK;
}

_Use_decl_annotations_
ID3D12FunctionReflection *DxilLibraryReflection::GetFunctionByIndex(INT FunctionIndex) {
  if ((UINT)FunctionIndex >= m_FunctionVector.size())
    return &g_InvalidFunction;
  return m_FunctionVector[FunctionIndex];
}

// DxilRuntimeReflection implementation
#include "dxc/HLSL/DxilRuntimeReflection.inl"

#else
void hlsl::CreateDxcContainerReflection(IDxcContainerReflection **ppResult) {
  *ppResult = nullptr;
}

DEFINE_CROSS_PLATFORM_UUIDOF(IDxcContainerReflection)
#endif // LLVM_ON_WIN32
