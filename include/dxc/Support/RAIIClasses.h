///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RAIIClasses.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// RAII Helpers.                                                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "WinIncludes.h"

class HhEvent {
    public:
      HANDLE m_handle = INVALID_HANDLE_VALUE;
    
    public:
      HRESULT Init() {
        if (m_handle == INVALID_HANDLE_VALUE) {
          m_handle = CreateEvent(nullptr, TRUE, FALSE, nullptr);
          if (m_handle == INVALID_HANDLE_VALUE) {
            return HRESULT_FROM_WIN32(GetLastError());
          }
        }
        return S_OK;
      }
      HANDLE Get() { return m_handle; }
      void SetEvent() { ::SetEvent(m_handle); }
      void ResetEvent() { ::ResetEvent(m_handle); }
      ~HhEvent() {
        if (m_handle != INVALID_HANDLE_VALUE) {
          CloseHandle(m_handle);
        }
      }
    };
    
    class HhCriticalSection {
    private:
      CRITICAL_SECTION m_cs;
    
    public:
      HhCriticalSection() { InitializeCriticalSection(&m_cs); }
      ~HhCriticalSection() { DeleteCriticalSection(&m_cs); }
      class Lock {
      private:
        CRITICAL_SECTION *m_pLock;
    
      public:
        Lock() = delete;
        Lock(const Lock &) = delete;
        Lock(Lock &&other) { std::swap(m_pLock, other.m_pLock); }
        Lock(CRITICAL_SECTION *pLock) : m_pLock(pLock) {
          EnterCriticalSection(m_pLock);
        }
        ~Lock() {
          if (m_pLock)
            LeaveCriticalSection(m_pLock);
        }
      };
      Lock LockCS() { return Lock(&m_cs); }
    };
    
    class ClassObjectRegistration {
    private:
      DWORD m_reg = 0;
      HRESULT m_hr = E_FAIL;
    
    public:
      HRESULT Register(REFCLSID rclsid, IUnknown *pUnk, DWORD dwClsContext,
                       DWORD flags) {
        m_hr = CoRegisterClassObject(rclsid, pUnk, dwClsContext, flags, &m_reg);
        return m_hr;
      }
      ~ClassObjectRegistration() {
        if (SUCCEEDED(m_hr))
          CoRevokeClassObject(m_reg);
      }
    };
    
    class CoInit {
    private:
      HRESULT m_hr = E_FAIL;
    
    public:
      HRESULT Initialize(DWORD dwCoInit) {
        m_hr = CoInitializeEx(nullptr, dwCoInit);
        return m_hr;
      }
      ~CoInit() {
        if (SUCCEEDED(m_hr))
          CoUninitialize();
      }
    };
    