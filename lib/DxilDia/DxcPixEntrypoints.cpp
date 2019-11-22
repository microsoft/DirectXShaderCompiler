///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcPixEntrypoints.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines all of the entrypoints for DXC's PIX interfaces for dealing with  //
// debug info. These entrypoints are responsible for setting up a common,    //
// sane environment -- e.g., exceptions are caught and returned as an        //
// HRESULT -- deferring to the real implementations defined elsewhere in     //
// this library.                                                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#include "dxc/dxcapi.h"
#include "dxc/dxcpix.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/FileSystem.h"

#include "DxcPixBase.h"
#include "DxcPixDxilDebugInfo.h"

#include <functional>

namespace dxil_debug_info
{
namespace entrypoints
{
// OutParam/OutParamImpl provides a mechanism that entrypoints
// use to tag method arguments as OutParams. OutParams are
// automatically zero-initialized.
template <typename T>
class OutParamImpl
{
public:
  OutParamImpl(T *V) : m_V(V)
  {
    if (m_V != nullptr)
    {
      *m_V = T();
    }
  }

  operator T *() const
  {
    return m_V;
  }

private:
  T *m_V;
};

template <typename T>
OutParamImpl<T> OutParam(T *V)
{
  return OutParamImpl<T>(V);
}

namespace check
{
// NotNull/NotNullImpl provide a mechanism that entrypoints can
// use for automatic parameter validation. They will throw an
// exception if the given parameter is null.
template <typename T>
class NotNullImpl
{
public:
  explicit NotNullImpl(T *V) : m_V(V)
  {
    if (m_V != nullptr)
    {
      *m_V = T();
    }
  }

  operator T *() const
  {
    if (m_V == nullptr)
    {
      throw hlsl::Exception(E_INVALIDARG);
    }
    return m_V;
  }

private:
  T *m_V;
};

template <typename T>
class NotNullImpl<const T>
{
public:
  explicit NotNullImpl(const T * V) : m_V(V)
  {
  }

  operator const T* () const
  {
    if (m_V == nullptr)
    {
      throw hlsl::Exception(E_INVALIDARG);
    }
    return m_V;
  }

private:
  const T* m_V;
};

template <typename T>
class NotNullImpl<OutParamImpl<T>>
{
public:
  explicit NotNullImpl(OutParamImpl<T> V) : m_V(V)
  {
  }

  operator T *() const
  {
    if (m_V == nullptr)
    {
      throw hlsl::Exception(E_INVALIDARG);
    }
    return m_V;
  }

private:
  T *m_V;
};

template <typename T>
NotNullImpl<T> NotNull(T *V)
{
  return NotNullImpl<T>(V);
}

template <typename T>
NotNullImpl<OutParamImpl<T>> NotNull(OutParamImpl<T> V)
{
  return NotNullImpl<OutParamImpl<T>>(V);
}
}  // namespace check

// WrapOutParams will wrap any OutParams<T> that the
// entrypoints provide to SetupAndRun -- which is essentially
// the method that runs the actual methods.
void WrapOutParams(IMalloc *)
{
}

template <typename T, typename... O>
void WrapOutParams(IMalloc *M, T, O... Others)
{
  WrapOutParams(M, Others...);
}

// DEFINE_WRAP_OUT_PARAM is a helper macro that every entrypoint should
// use in order to define a WrapOutParams function to wrap COM objects
// with the entrypoints wrapper.
#define DEFINE_WRAP_OUT_PARAM(IInterface)                                   \
template <typename... R>                                                    \
void WrapOutParams(                                                         \
    IMalloc *M,                                                             \
    check::NotNullImpl<OutParamImpl<IInterface *>> ppOut,                   \
    R... Results)                                                           \
{                                                                           \
  if (*ppOut)                                                               \
  {                                                                         \
    NewDxcPixDxilDebugInfoObjectOrThrow<IInterface##Entrypoint>(            \
        (IInterface **)ppOut,                                               \
        M,                                                                  \
        *ppOut);                                                            \
  }                                                                         \
  WrapOutParams(M, Results...);                                             \
}

// SetupAndRun is the function that sets up the environment
// in which all of the user requests to the DxcPix library
// run under.
template<typename H, typename... A>
HRESULT SetupAndRun(
    IMalloc* M,
    H Handler,
    A... Args
)
{
  DxcThreadMalloc TM(M);

  HRESULT hr = S_FALSE;
  try
  {
    ::llvm::sys::fs::MSFileSystem* msfPtr;
    IFT(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    hr = Handler(Args...);
    WrapOutParams(M, Args...);
  }
  catch (const hlsl::Exception &e)
  {
      hr = e.hr;
  }
  catch (const std::bad_alloc &)
  {
      hr = E_OUTOFMEMORY;
  }
  catch (const std::exception &)
  {
      hr = E_FAIL;
  }

  return hr;
}

HRESULT CreateEntrypointWrapper(
    IMalloc *pMalloc,
    IUnknown *pReal,
    REFIID iid,
    void **ppvObject);

// Entrypoint is the base class for all entrypoints, providing
// the default QueryInterface implementation, as well as a
// more convenient way of calling SetupAndRun.
template <typename I>
class Entrypoint : public I
{
protected:
  using IInterface = I;

  Entrypoint(
      IMalloc *pMalloc,
      IInterface *pI
  ) : m_pMalloc(pMalloc)
    , m_pReal(pI)
  {
  }

  DXC_MICROCOM_TM_REF_FIELDS();
  CComPtr<IInterface> m_pReal;

  template <typename F, typename... A>
  HRESULT InvokeOnReal(F pFn, A... Args)
  {
    return ::SetupAndRun(m_pMalloc, std::mem_fn(pFn), m_pReal, Args...);
  }

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override final
  {
    return ::SetupAndRun(
        m_pMalloc,
        std::mem_fn(&Entrypoint<IInterface>::QueryInterfaceImpl),
        this,
        iid,
        check::NotNull(OutParam(ppvObject)));
  }

  HRESULT STDMETHODCALLTYPE QueryInterfaceImpl(REFIID iid, void** ppvObject)
  {
    // Special-casing so we don't need to create a new wrapper.
    if (iid == __uuidof(IInterface) ||
        iid == __uuidof(IUnknown)   ||
        iid == __uuidof(INoMarshal))
    {
      this->AddRef();
      *ppvObject = this;
      return S_OK;
    }

    CComPtr<IUnknown> RealQI;
    IFR(m_pReal->QueryInterface(iid, (void**)&RealQI));
    return CreateEntrypointWrapper(m_pMalloc, RealQI, iid, ppvObject);
  }
};

#define DEFINE_ENTRYPOINT_BOILERPLATE(Name)                                  \
    Name(IMalloc *M, IInterface *pI) : Entrypoint<IInterface>(M, pI){}       \
    DXC_MICROCOM_TM_ALLOC(Name)

struct IUnknownEntrypoint : public Entrypoint<IUnknown>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IUnknownEntrypoint);
};
DEFINE_WRAP_OUT_PARAM(IUnknown);

struct IDxcPixTypeEntrypoint : public Entrypoint<IDxcPixType>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixTypeEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetSizeInBits(
      _Outptr_result_z_ DWORD *SizeInBits) override
  {
    return InvokeOnReal(&IInterface::GetSizeInBits, check::NotNull(OutParam(SizeInBits)));
  }

  STDMETHODIMP UnAlias(
      _Outptr_result_z_ IDxcPixType** ppBaseType) override
  {
    return InvokeOnReal(&IInterface::UnAlias, check::NotNull(OutParam(ppBaseType)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixType);

struct IDxcPixConstTypeEntrypoint : public Entrypoint<IDxcPixConstType>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixConstTypeEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetSizeInBits(
      _Outptr_result_z_ DWORD *SizeInBits) override
  {
    return InvokeOnReal(&IInterface::GetSizeInBits, check::NotNull(OutParam(SizeInBits)));
  }

  STDMETHODIMP UnAlias(
      _Outptr_result_z_ IDxcPixType** ppBaseType) override
  {
    return InvokeOnReal(&IInterface::UnAlias, check::NotNull(OutParam(ppBaseType)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixConstType);

struct IDxcPixTypedefTypeEntrypoint : public Entrypoint<IDxcPixTypedefType>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixTypedefTypeEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetSizeInBits(
      _Outptr_result_z_ DWORD *SizeInBits) override
  {
    return InvokeOnReal(&IInterface::GetSizeInBits, check::NotNull(OutParam(SizeInBits)));
  }

  STDMETHODIMP UnAlias(
      _Outptr_result_z_ IDxcPixType** ppBaseType) override
  {
    return InvokeOnReal(&IInterface::UnAlias, check::NotNull(OutParam(ppBaseType)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixTypedefType);

struct IDxcPixScalarTypeEntrypoint : public Entrypoint<IDxcPixScalarType>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixScalarTypeEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetSizeInBits(
      _Outptr_result_z_ DWORD *SizeInBits) override
  {
    return InvokeOnReal(&IInterface::GetSizeInBits, check::NotNull(OutParam(SizeInBits)));
  }

  STDMETHODIMP UnAlias(
      _Outptr_result_z_ IDxcPixType** ppBaseType) override
  {
    return InvokeOnReal(&IInterface::UnAlias, check::NotNull(OutParam(ppBaseType)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixScalarType);

struct IDxcPixArrayTypeEntrypoint : public Entrypoint<IDxcPixArrayType>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixArrayTypeEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetSizeInBits(
      _Outptr_result_z_ DWORD *SizeInBits) override
  {
    return InvokeOnReal(&IInterface::GetSizeInBits, check::NotNull(OutParam(SizeInBits)));
  }

  STDMETHODIMP UnAlias(
      _Outptr_result_z_ IDxcPixType** ppBaseType) override
  {
    return InvokeOnReal(&IInterface::UnAlias, check::NotNull(OutParam(ppBaseType)));
  }

  STDMETHODIMP GetNumElements(
      _Outptr_result_z_ DWORD *ppNumElements) override
  {
    return InvokeOnReal(&IInterface::GetNumElements, check::NotNull(OutParam(ppNumElements)));
  }

  STDMETHODIMP GetIndexedType(
      _Outptr_result_z_ IDxcPixType **ppElementType) override
  {
    return InvokeOnReal(&IInterface::GetIndexedType, check::NotNull(OutParam(ppElementType)));
  }

  STDMETHODIMP GetElementType(
      _Outptr_result_z_ IDxcPixType** ppElementType) override
  {
    return InvokeOnReal(&IInterface::GetElementType, check::NotNull(OutParam(ppElementType)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixArrayType);

struct IDxcPixStructFieldEntrypoint : public Entrypoint<IDxcPixStructField>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixStructFieldEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetType(
      _Outptr_result_z_ IDxcPixType** ppType) override
  {
    return InvokeOnReal(&IInterface::GetType, check::NotNull(OutParam(ppType)));
  }

  STDMETHODIMP GetOffsetInBits(
      _Outptr_result_z_ DWORD *pOffsetInBits) override
  {
    return InvokeOnReal(&IInterface::GetOffsetInBits, check::NotNull(OutParam(pOffsetInBits)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixStructField);

struct IDxcPixStructTypeEntrypoint : public Entrypoint<IDxcPixStructType>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixStructTypeEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetSizeInBits(
      _Outptr_result_z_ DWORD *SizeInBits) override
  {
    return InvokeOnReal(&IInterface::GetSizeInBits, check::NotNull(OutParam(SizeInBits)));
  }

  STDMETHODIMP UnAlias(
      _Outptr_result_z_ IDxcPixType** ppBaseType) override
  {
    return InvokeOnReal(&IInterface::UnAlias, check::NotNull(OutParam(ppBaseType)));
  }

  STDMETHODIMP GetNumFields(
      _Outptr_result_z_ DWORD* ppNumFields) override
  {
      return InvokeOnReal(&IInterface::GetNumFields, check::NotNull(OutParam(ppNumFields)));
  }

  STDMETHODIMP GetFieldByIndex(
      DWORD dwIndex,
      _Outptr_result_z_ IDxcPixStructField **ppField) override
  {
    return InvokeOnReal(&IInterface::GetFieldByIndex, dwIndex, check::NotNull(OutParam(ppField)));
  }

  STDMETHODIMP GetFieldByName(
      _In_ LPCWSTR lpName,
      _Outptr_result_z_ IDxcPixStructField** ppField) override
  {
    return InvokeOnReal(&IInterface::GetFieldByName, check::NotNull(lpName), check::NotNull(OutParam(ppField)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixStructType);

struct IDxcPixDxilStorageEntrypoint : public Entrypoint<IDxcPixDxilStorage>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixDxilStorageEntrypoint);

  STDMETHODIMP AccessField(
      _In_ LPCWSTR Name,
      _COM_Outptr_ IDxcPixDxilStorage** ppResult) override
  {
    return InvokeOnReal(&IInterface::AccessField, check::NotNull(Name), check::NotNull(OutParam(ppResult)));
  }

  STDMETHODIMP Index(
      _In_ DWORD Index,
      _COM_Outptr_ IDxcPixDxilStorage** ppResult) override
  {
    return InvokeOnReal(&IInterface::Index, Index, check::NotNull(OutParam(ppResult)));
  }

  STDMETHODIMP GetRegisterNumber(
      _Outptr_result_z_ DWORD *pRegNum) override
  {
    return InvokeOnReal(&IInterface::GetRegisterNumber, check::NotNull(OutParam(pRegNum)));
  }

  STDMETHODIMP GetIsAlive() override
  {
    return InvokeOnReal(&IInterface::GetIsAlive);
  }

  STDMETHODIMP GetType(
      _Outptr_result_z_ IDxcPixType** ppType) override
  {
    return InvokeOnReal(&IInterface::GetType, check::NotNull(OutParam(ppType)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixDxilStorage);

struct IDxcPixVariableEntrypoint : public Entrypoint<IDxcPixVariable>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixVariableEntrypoint);

  STDMETHODIMP GetName(
      _Outptr_result_z_ BSTR *Name) override
  {
    return InvokeOnReal(&IInterface::GetName, check::NotNull(OutParam(Name)));
  }

  STDMETHODIMP GetType(
      _Outptr_result_z_ IDxcPixType **ppType) override
  {
    return InvokeOnReal(&IInterface::GetType, check::NotNull(OutParam(ppType)));
  }

  STDMETHODIMP GetStorage(
      _COM_Outptr_ IDxcPixDxilStorage **ppStorage) override
  {
      return InvokeOnReal(&IInterface::GetStorage, check::NotNull(OutParam(ppStorage)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixVariable);

struct IDxcPixDxilLiveVariablesEntrypoint : public Entrypoint<IDxcPixDxilLiveVariables>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixDxilLiveVariablesEntrypoint);

  STDMETHODIMP GetCount(
      _Outptr_ DWORD *dwSize) override
  {
    return InvokeOnReal(&IInterface::GetCount, check::NotNull(OutParam(dwSize)));
  }

  STDMETHODIMP GetVariableByIndex(
      _In_ DWORD Index,
      _Outptr_result_z_ IDxcPixVariable ** ppVariable) override
  {
    return InvokeOnReal(&IInterface::GetVariableByIndex, Index, check::NotNull(OutParam(ppVariable)));
  }

  STDMETHODIMP GetVariableByName(
      _In_ LPCWSTR Name,
      _Outptr_result_z_ IDxcPixVariable** ppVariable) override
  {
    return InvokeOnReal(&IInterface::GetVariableByName, check::NotNull(Name), check::NotNull(OutParam(ppVariable)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixDxilLiveVariables);

struct IDxcPixDxilDebugInfoEntrypoint : public Entrypoint<IDxcPixDxilDebugInfo>
{
  DEFINE_ENTRYPOINT_BOILERPLATE(IDxcPixDxilDebugInfoEntrypoint);

  STDMETHODIMP GetLiveVariablesAt(
      _In_ DWORD InstructionOffset,
      _COM_Outptr_ IDxcPixDxilLiveVariables **ppLiveVariables) override
  {
    return InvokeOnReal(&IInterface::GetLiveVariablesAt, InstructionOffset, check::NotNull(OutParam(ppLiveVariables)));
  }

  STDMETHODIMP IsVariableInRegister(
      _In_ DWORD InstructionOffset,
      _In_ const wchar_t *VariableName) override
  {
    return InvokeOnReal(&IInterface::IsVariableInRegister, InstructionOffset, check::NotNull(VariableName));
  }

  STDMETHODIMP GetFunctionName(
      _In_ DWORD InstructionOffset,
      _Outptr_result_z_ BSTR *ppFunctionName) override
  {
    return InvokeOnReal(&IInterface::GetFunctionName, InstructionOffset, check::NotNull(OutParam(ppFunctionName)));
  }

  STDMETHODIMP GetStackDepth(
      _In_ DWORD InstructionOffset,
      _Outptr_ DWORD *StackDepth) override
  {
    return InvokeOnReal(&IInterface::GetStackDepth, InstructionOffset, check::NotNull(OutParam(StackDepth)));
  }
};
DEFINE_WRAP_OUT_PARAM(IDxcPixDxilDebugInfo);

HRESULT CreateEntrypointWrapper(
    IMalloc* pMalloc,
    IUnknown* pReal,
    REFIID riid,
    void** ppvObject)
{
#define HANDLE_INTERFACE(IInterface)                                         \
    if (__uuidof(IInterface) == riid)                                        \
    {                                                                        \
        return NewDxcPixDxilDebugInfoObjectOrThrow<IInterface##Entrypoint>(  \
            (IInterface **) ppvObject,                                       \
            pMalloc,                                                         \
            (IInterface *) pReal);                                           \
    } (void)0

  HANDLE_INTERFACE(IUnknown);
  HANDLE_INTERFACE(IDxcPixType);
  HANDLE_INTERFACE(IDxcPixConstType);
  HANDLE_INTERFACE(IDxcPixTypedefType);
  HANDLE_INTERFACE(IDxcPixScalarType);
  HANDLE_INTERFACE(IDxcPixArrayType);
  HANDLE_INTERFACE(IDxcPixStructField);
  HANDLE_INTERFACE(IDxcPixStructType);
  HANDLE_INTERFACE(IDxcPixDxilStorage);
  HANDLE_INTERFACE(IDxcPixVariable);
  HANDLE_INTERFACE(IDxcPixDxilLiveVariables);
  HANDLE_INTERFACE(IDxcPixDxilDebugInfo);

  return E_FAIL;
}

}  // namespace entrypoints
}  // namespace dxil_debug_info

#include "DxilDiaSession.h"

using namespace dxil_debug_info::entrypoints;
static STDMETHODIMP NewDxcPixDxilDebugInfoImpl(
    IMalloc *pMalloc,
    dxil_dia::Session *pSession,
    IDxcPixDxilDebugInfo** ppDxilDebugInfo
)
{
  return dxil_debug_info::NewDxcPixDxilDebugInfoObjectOrThrow<dxil_debug_info::DxcPixDxilDebugInfo>(
      ppDxilDebugInfo,
      pMalloc,
      pSession);
}

STDMETHODIMP dxil_dia::Session::NewDxcPixDxilDebugInfo(
    _COM_Outptr_ IDxcPixDxilDebugInfo** ppDxilDebugInfo)
{
  return SetupAndRun(
      m_pMalloc,
      &NewDxcPixDxilDebugInfoImpl,
      m_pMalloc,
      this,
      check::NotNull(OutParam(ppDxilDebugInfo)));
}
