///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPDB.h                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helpers to wrap debug information in a PDB container.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Endian.h"

struct IDxcBlob;
struct IStream;
struct IMalloc;

namespace hlsl {
namespace pdb {

// MSF header
static const char kMsfMagic[] = {
    'M', 'i', 'c',  'r',  'o',    's', 'o', 'f',  't',  ' ', 'C',
    '/', 'C', '+',  '+',  ' ',    'M', 'S', 'F',  ' ',  '7', '.',
    '0', '0', '\r', '\n', '\x1a', 'D', 'S', '\0', '\0', '\0'};

// The superblock is overlaid at the beginning of the file (offset 0).
// It starts with a magic header and is followed by information which
// describes the layout of the file system.
struct MSF_SuperBlock {
  char MagicBytes[sizeof(kMsfMagic)];
  // The file system is split into a variable number of fixed size elements.
  // These elements are referred to as blocks.  The size of a block may vary
  // from system to system.
  llvm::support::ulittle32_t BlockSize;
  // The index of the free block map.
  llvm::support::ulittle32_t FreeBlockMapBlock;
  // This contains the number of blocks resident in the file system.  In
  // practice, NumBlocks * BlockSize is equivalent to the size of the MSF
  // file.
  llvm::support::ulittle32_t NumBlocks;
  // This contains the number of bytes which make up the directory.
  llvm::support::ulittle32_t NumDirectoryBytes;
  // This field's purpose is not yet known.
  llvm::support::ulittle32_t Unknown1;
  // This contains the block # of the block map.
  llvm::support::ulittle32_t BlockMapAddr;
};

HRESULT LoadDataFromStream(IMalloc *pMalloc, IStream *pIStream,
                           IDxcBlob **ppHash, IDxcBlob **ppContainer);
HRESULT LoadDataFromStream(IMalloc *pMalloc, IStream *pIStream,
                           IDxcBlob **pOutContainer);
HRESULT WriteDxilPDB(IMalloc *pMalloc, IDxcBlob *pContainer,
                     llvm::ArrayRef<BYTE> HashData, IDxcBlob **ppOutBlob);
HRESULT WriteDxilPDB(IMalloc *pMalloc, llvm::ArrayRef<BYTE> ContainerData,
                     llvm::ArrayRef<BYTE> HashData, IDxcBlob **ppOutBlob);
} // namespace pdb
} // namespace hlsl
