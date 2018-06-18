//===- WinIncludes.h --------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// WinIncludes.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#define NOATOM 1
#define NOGDICAPMASKS 1
#define NOMETAFILE 1
#define NOMINMAX 1
#define NOOPENFILE 1
#define NORASTEROPS 1
#define NOSCROLL 1
#define NOSOUND 1
#define NOSYSMETRICS 1
#define NOWH 1
#define NOCOMM 1
#define NOKANJI 1
#define NOCRYPT 1
#define NOMCX 1
#define WIN32_LEAN_AND_MEAN 1
#define VC_EXTRALEAN 1
#define NONAMELESSSTRUCT 1

#include <windows.h>
#include <unknwn.h>
#include <atlbase.h> // atlbase.h needs to come before strsafe.h
#include <strsafe.h>
#include <intsafe.h>
#include <ObjIdl.h>

// Support older atlbase.h if needed
#ifndef _ATL_DECLSPEC_ALLOCATOR
#define _ATL_DECLSPEC_ALLOCATOR
#endif

/// Swap two ComPtr classes.
template <class T> void swap(CComHeapPtr<T> &a, CComHeapPtr<T> &b) {
  T *c(a.m_pData);
  a.m_pData = b.m_pData;
  b.m_pData = c;
}
