///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerReader.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helpers for reading from dxil container.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace hlsl {

class DxilSubobjects;
namespace RDAT {
  class SubobjectTableReader;
}

bool LoadSubobjectsFromRDAT(DxilSubobjects &subobjects,
  RDAT::SubobjectTableReader *pSubobjectTableReader);

} // namespace hlsl