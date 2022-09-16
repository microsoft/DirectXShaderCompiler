///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RDAT_PdbInfoTypes.inl                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines macros use to define types Dxil PDBInfo data.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifdef DEF_RDAT_TYPES

#define RECORD_TYPE DxilSubPdbInfo
RDAT_STRUCT_TABLE(DxilSubPdbInfo, DxilSubPdbInfoTable)
  RDAT_BYTES(Data)
RDAT_STRUCT_END()
#undef RECORD_TYPE

#define RECORD_TYPE DxilPdbInfo
RDAT_STRUCT_TABLE(DxilPdbInfo, DxilPdbInfoTable)
  RDAT_STRING(Entry)
  RDAT_STRING(TargetProfile)
  RDAT_STRING(MainFileName)
  RDAT_STRING_ARRAY_REF(SourceNames)
  RDAT_STRING_ARRAY_REF(SourceContents)
  RDAT_STRING_ARRAY_REF(ArgPairs)
  RDAT_RECORD_ARRAY_REF(DxilSubPdbInfo, Libraries)
  RDAT_STRING_ARRAY_REF(LibraryNames)
  RDAT_BYTES(Hash)
  RDAT_STRING(PdbName)
  RDAT_VALUE(uint32_t, CustomToolchainId)
  RDAT_BYTES(CustomToolchainData)
  RDAT_BYTES(WholeDxil)
RDAT_STRUCT_END()
#undef RECORD_TYPE

#endif // DEF_RDAT_TYPES


