///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxc.h                                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides wrappers to dxc main function.                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef __DXC_DXCLIB__
#define __DXC_DXCLIB__

namespace dxc
{
#ifdef _WIN32
int main(int argc, const wchar_t **argv_);
#else
int main(int argc, const char **argv_);
#endif // _WIN32
}

#endif // __DXC_DXCLIB__