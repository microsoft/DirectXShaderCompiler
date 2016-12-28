///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// microcom.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Provides support for basic COM-like constructs.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXC_MICROCOM__
#define __DXC_MICROCOM__

template <typename TIface>
class CComInterfaceArray {
private:
  TIface **m_pData;
  unsigned m_length;
public:
  CComInterfaceArray() : m_pData(nullptr), m_length(0) { }
  ~CComInterfaceArray() {
    clear();
  }
  bool empty() const { return m_length == 0; }
  unsigned size() const { return m_length; }
  TIface ***data_ref() { return &m_pData; }
  unsigned *size_ref() { return &m_length; }
  TIface **begin() {
    return m_pData;
  }
  TIface **end() {
    return m_pData + m_length;
  }
  void clear() {
    if (m_length) {
      for (unsigned i = 0; i < m_length; ++i) {
        if (m_pData[i] != nullptr) {
          m_pData[i]->Release();
          m_pData[i] = nullptr;
        }
      }
      m_length = 0;
    }
    if (m_pData) {
      CoTaskMemFree(m_pData);
      m_pData = nullptr;
    }
  }
  HRESULT alloc(unsigned count) {
    clear();
    m_pData = (TIface**)CoTaskMemAlloc(sizeof(TIface*) * count);
    if (m_pData == nullptr)
      return E_OUTOFMEMORY;
    m_length = count;
    ZeroMemory(m_pData, sizeof(TIface*) * count);
    return S_OK;
  }
  TIface **get_address_of(unsigned index) {
    return &(m_pData[index]);
  }
  TIface **release() {
    TIface **result = m_pData;
    m_pData = nullptr;
    m_length = 0;
    return result;
  }
  void release(TIface ***pValues, unsigned *length) {
    *pValues = m_pData;
    m_pData = nullptr;
    *length = m_length;
    m_length = 0;
  }
};

#define DXC_MICROCOM_REF_FIELD(m_dwRef) volatile ULONG m_dwRef;
    
#define DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef) \
    bool HasSingleRef() { return 1 == m_dwRef; } \
    ULONG STDMETHODCALLTYPE AddRef()  {\
        return InterlockedIncrement(&m_dwRef); \
    } \
    ULONG STDMETHODCALLTYPE Release() { \
        ULONG result = InterlockedDecrement(&m_dwRef); \
        if (result == 0) delete this; \
        return result; \
    }

/// <summary>
/// Provides a QueryInterface implementation for a class that supports
/// a single interface in addition to IUnknown.
/// </summary>
/// <remarks>
/// This implementation will also report the instance as not supporting
/// marshaling. This will help catch marshaling problems early or avoid
/// them altogether.
/// </remarks>
template <typename TInterface, typename TObject>
HRESULT DoBasicQueryInterface(TObject* self, REFIID iid, void** ppvObject)
{
  if (ppvObject == nullptr) return E_POINTER;

  // Support INoMarshal to void GIT shenanigans.
  if (IsEqualIID(iid, __uuidof(IUnknown)) ||
    IsEqualIID(iid, __uuidof(INoMarshal))) {
    *ppvObject = reinterpret_cast<IUnknown*>(self);
    reinterpret_cast<IUnknown*>(self)->AddRef();
    return S_OK;
  }

  if (IsEqualIID(iid, __uuidof(TInterface))) {
    *(TInterface**)ppvObject = self;
    self->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

/// <summary>
/// Provides a QueryInterface implementation for a class that supports
/// two interfaces in addition to IUnknown.
/// </summary>
/// <remarks>
/// This implementation will also report the instance as not supporting
/// marshaling. This will help catch marshaling problems early or avoid
/// them altogether.
/// </remarks>
template <typename TInterface, typename TInterface2, typename TObject>
HRESULT DoBasicQueryInterface2(TObject* self, REFIID iid, void** ppvObject)
{
  if (ppvObject == nullptr) return E_POINTER;

  // Support INoMarshal to void GIT shenanigans.
  if (IsEqualIID(iid, __uuidof(IUnknown)) ||
      IsEqualIID(iid, __uuidof(INoMarshal))) {
    *ppvObject = reinterpret_cast<IUnknown*>(self);
    reinterpret_cast<IUnknown*>(self)->AddRef();
    return S_OK;
  }

  if (IsEqualIID(__uuidof(TInterface), iid)) {
    *(TInterface**)ppvObject = self;
    self->AddRef();
    return S_OK;
  }

  if (IsEqualIID(__uuidof(TInterface2), iid)) {
    *(TInterface2**)ppvObject = self;
    self->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

/// <summary>
/// Provides a QueryInterface implementation for a class that supports
/// three interfaces in addition to IUnknown.
/// </summary>
/// <remarks>
/// This implementation will also report the instance as not supporting
/// marshaling. This will help catch marshaling problems early or avoid
/// them altogether.
/// </remarks>
template <typename TInterface, typename TInterface2, typename TInterface3, typename TObject>
HRESULT DoBasicQueryInterface3(TObject* self, REFIID iid, void** ppvObject)
{
  if (ppvObject == nullptr) return E_POINTER;
  if (IsEqualIID(iid, __uuidof(TInterface3))) {
    *(TInterface3**)ppvObject = self;
    self->AddRef();
    return S_OK;
  }

  return DoBasicQueryInterface2<TInterface, TInterface2, TObject>(self, iid, ppvObject);
}

template <typename T>
HRESULT AssignToOut(T value, _Out_ T* pResult) {
  if (pResult == nullptr)
    return E_POINTER;
  *pResult = value;
  return S_OK;
}
template <typename T>
HRESULT AssignToOut(nullptr_t value, _Out_ T* pResult) {
  if (pResult == nullptr)
    return E_POINTER;
  *pResult = value;
  return S_OK;
}
template <typename T>
HRESULT ZeroMemoryToOut(_Out_ T* pResult) {
  if (pResult == nullptr)
    return E_POINTER;
  ZeroMemory(pResult, sizeof(*pResult));
  return S_OK;
}

template <typename T>
void AssignToOutOpt(T value, _Out_opt_ T* pResult) {
  if (pResult != nullptr)
    *pResult = value;
}
template <typename T>
void AssignToOutOpt(nullptr_t value, _Out_opt_ T* pResult) {
  if (pResult != nullptr)
    *pResult = value;
}

#endif
