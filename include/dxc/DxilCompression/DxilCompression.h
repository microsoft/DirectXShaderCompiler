///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCompression.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//
// Helper wrapper functions for zlib deflate and inflate. Entirely
// self-contained, only depends on IMalloc interface.
//

#pragma once

struct IMalloc;
namespace hlsl {
  enum class ZlibResult {
    Success = 0,
    InvalidData = 1,
    OutOfMemory = 2,
  };

  ZlibResult ZlibDecompress(
    IMalloc *pMalloc,
    const void *pCompressedBuffer,
    size_t BufferSizeInBytes,
    void *pUncompressedBuffer,
    size_t UncompressedBufferSize);

  typedef void *ZlibAllocateBufferFn(void *pUserData, size_t RequiredSize);
  ZlibResult ZlibCompress(IMalloc *pMalloc,
    const void *pData, size_t pDataSize,
    void *pUserData,
    ZlibAllocateBufferFn *AllocateBufferFn,
    size_t *pOutCompressedSize);
}
