///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcshadersourceinfo.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Utility helpers for dealing with DXIL part related to shader sources      //
// and options.                                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxcshadersourceinfo.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "llvm/Support/Path.h"
#include "miniz.h"

using namespace hlsl;
using Buffer = SourceInfoWriter::Buffer;

///////////////////////////////////////////////////////////////////////////////
// Reader
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Writer
///////////////////////////////////////////////////////////////////////////////

static void Append(Buffer *buf, uint8_t c) {
  buf->push_back(c);
}
static void Append(Buffer *buf, const void *ptr, size_t size) {
  size_t OldSize = buf->size();
  buf->resize(OldSize + size);
  memcpy(buf->data() + OldSize, ptr, size);
}

static uint32_t PadToFourBytes(uint32_t size) {
  uint32_t rem = size % 4;
  if (rem)
    return size + (4 - rem);
  return size;
}

static uint32_t PadBufferToFourBytes(Buffer *buf, uint32_t unpaddedSize) {
  const uint32_t paddedSize = PadToFourBytes(unpaddedSize);
  // Padding.
  for (uint32_t i = unpaddedSize; i < paddedSize; i++) {
    Append(buf, 0);
  }
  return paddedSize;
}

static void AppendFileEntry(Buffer *buf, llvm::StringRef name, llvm::StringRef content) {
  hlsl::DxilShaderSourcesElement header = {};
  header.ContentSize = content.size();
  header.NameSize = name.size();
  header.SizeInDwords = PadToFourBytes(sizeof(header) + name.size()+1 + content.size()+1) / sizeof(uint32_t);

  const size_t offset = buf->size();
  Append(buf, &header, sizeof(header));
  Append(buf, name.data(), name.size());
  Append(buf, 0); // Null term
  Append(buf, content.data(), content.size());
  Append(buf, 0); // Null term

  const size_t paddedOffset = PadBufferToFourBytes(buf, buf->size() - offset);
  (void)paddedOffset;
  assert(paddedOffset == header.SizeInDwords*sizeof(uint32_t));
}

static bool ZlibCompress(const void *src, size_t srcSize, Buffer *outCompressedData) {
  Buffer &compressedData = *outCompressedData;

  struct Stream_Storage {
    z_stream stream = {};
    ~Stream_Storage() {
      deflateEnd(&stream);
    }
  };

  Stream_Storage storage;
  z_stream *stream = &storage.stream;

  constexpr size_t CHUNK_SIZE = 16 * 1024;
  constexpr int level = Z_DEFAULT_COMPRESSION;
  int status = Z_OK;

  stream->zalloc = [](void *opaque, size_t items, size_t size) -> void* {
    void *ret = nullptr;
    try {
      ret = new uint8_t[items * size];
    }
    catch (std::bad_alloc e) {
      return NULL;
    }
    return ret;
  };
  stream->zfree = [](void *opaque, void *address) -> void {
    delete[] (uint8_t *)address;
  };

  status = deflateInit(stream, level);
  if (Z_OK != status)
    return false;

  stream->next_in = (unsigned char *)src;
  stream->avail_in = (uInt)srcSize;

  for (;;) {
    uInt OldSize = compressedData.size();
    compressedData.resize(compressedData.size() + CHUNK_SIZE);

    stream->next_out = (unsigned char *)(compressedData.data() + OldSize);
    stream->avail_out = CHUNK_SIZE;

    status = deflate(stream, stream->avail_in != 0 ? Z_NO_FLUSH : Z_FINISH);

    if (status != Z_OK && status != Z_STREAM_END)
      return false;

    // Have to shrink the buffer every time we have some left over, because even
    // if the stream wasn't written completely, it doesn't mean the compression
    // is over. Might have to do a MZ_FINISH, which could write more data.
    if (stream->avail_out) {
      compressedData.resize(compressedData.size() - stream->avail_out);
    }

    if (Z_STREAM_END == status)
      break;
  }

  if (Z_STREAM_END != status)
    return false;

  return true;
}

void SourceInfoWriter::Write(clang::CodeGenOptions &cgOpts, clang::SourceManager &srcMgr) {
  m_Buffer.clear();

  // Write an empty header first.
  hlsl::DxilShaderSourceInfo mainHeader = {};
  Append(&m_Buffer, &mainHeader, sizeof(mainHeader));

  ////////////////////////////////////////////////////////////////////
  // Add all file contents in a list of filename/content pairs.
  ////////////////////////////////////////////////////////////////////
  {
    const size_t elementOffset = m_Buffer.size();

    Buffer uncompressedBuffer;
    std::map<std::string, llvm::StringRef> filesMap;
    {
      bool bFoundMainFile = false;
      for (auto it = srcMgr.fileinfo_begin(), end = srcMgr.fileinfo_end(); it != end; ++it) {
        if (it->first->isValid() && !it->second->IsSystemFile) {
          // If main file, write that to metadata first.
          // Add the rest to filesMap to sort by name.
          llvm::SmallString<128> NormalizedPath;
          llvm::sys::path::native(it->first->getName(), NormalizedPath);
          if (cgOpts.MainFileName.compare(it->first->getName()) == 0) {
            assert(!bFoundMainFile && "otherwise, more than one file matches main filename");
            AppendFileEntry(&uncompressedBuffer, NormalizedPath, it->second->getRawBuffer()->getBuffer());
            bFoundMainFile = true;
          } else {
            filesMap[NormalizedPath.str()] =
                it->second->getRawBuffer()->getBuffer();
          }
        }
      }
      assert(bFoundMainFile && "otherwise, no file found matches main filename");
      // Emit the rest of the files in sorted order.
      for (auto it : filesMap) {
        AppendFileEntry(&uncompressedBuffer, it.first, it.second);
      }
    }

    hlsl::DxilShaderSourceInfoElement elementHeader = {};
    Append(&m_Buffer, &elementHeader, sizeof(elementHeader)); // Write an empty header

    const size_t headerOffset = m_Buffer.size();

    // Write the header
    hlsl::DxilShaderSources header = {};
    header.UncompressedSizeInBytes = uncompressedBuffer.size();
    header.SizeInBytes = uncompressedBuffer.size();
    header.FileCount = filesMap.size() + 1;
    Append(&m_Buffer, &header, sizeof(header));

    const size_t contentOffset = m_Buffer.size();

    bool bCompress = true;
    bool bCompressed = false;
    if (bCompress) {
      bCompressed = ZlibCompress(uncompressedBuffer.data(), uncompressedBuffer.size(), &m_Buffer);
      if (!bCompressed)
        uncompressedBuffer.resize(contentOffset); // Reset the size back
    }

    // If we compressed the content, go back to rewrite the header to write the
    // correct size in bytes.
    if (bCompressed) {
      size_t compressedSize = m_Buffer.size() - contentOffset;
      header.SizeInBytes = compressedSize;
      header.CompressType = hlsl::DxilShaderSourceCompressType::Zlib;
      memcpy(m_Buffer.data() + headerOffset, &header, sizeof(header));
    }
    // Otherwise, just write the whole uncompressed
    else {
      Append(&m_Buffer, uncompressedBuffer.data(), uncompressedBuffer.size());
    }

    // Calculate and pad the size of the element.
    const uint32_t unpaddedElementSize = m_Buffer.size() - elementOffset;
    const uint32_t paddedElementSize = PadBufferToFourBytes(&m_Buffer, unpaddedElementSize);

    // Go back and rewrite the element header
    assert(paddedElementSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = paddedElementSize / 4;
    elementHeader.Type = hlsl::DxilShaderSourceElementType::Sources;
    memcpy(m_Buffer.data() + elementOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }

  ////////////////////////////////////////////////////////////////////
  // Defines
  ////////////////////////////////////////////////////////////////////
  {
    const size_t elementOffset = m_Buffer.size();

    hlsl::DxilShaderSourceInfoElement elementHeader = {};
    Append(&m_Buffer, &elementHeader, sizeof(elementHeader)); // Write an empty header

    hlsl::DxilShaderCompileOptions header = {};
    header.Count = cgOpts.HLSLDefines.size();

    Append(&m_Buffer, &header, sizeof(header));

    for (std::string &def : cgOpts.HLSLDefines) {
      Append(&m_Buffer, def.data(), def.size());
      Append(&m_Buffer, 0); // Null terminator
    }
    Append(&m_Buffer, 0); // Double null terminator

    // Calculate and pad the size of the element.
    uint32_t unpaddedElementSize = m_Buffer.size() - elementOffset;
    uint32_t elementSize = PadBufferToFourBytes(&m_Buffer, unpaddedElementSize);

    // Go back and rewrite the element header
    assert(elementSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = elementSize;
    elementHeader.Type = hlsl::DxilShaderSourceElementType::Defines;
    memcpy(m_Buffer.data() + elementOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }


  ////////////////////////////////////////////////////////////////////
  // Args
  ////////////////////////////////////////////////////////////////////
  {
    const size_t elementOffset = m_Buffer.size();

    hlsl::DxilShaderSourceInfoElement elementHeader = {};
    Append(&m_Buffer, &elementHeader, sizeof(elementHeader)); // Write an empty header

    hlsl::DxilShaderCompileOptions header = {};
    header.Count = cgOpts.HLSLArguments.size();

    Append(&m_Buffer, &header, sizeof(header));

    for (std::string &arg : cgOpts.HLSLArguments) {
      Append(&m_Buffer, arg.data(), arg.size());
      Append(&m_Buffer, 0); // Null terminator
    }
    Append(&m_Buffer, 0); // Double null terminator

    // Calculate and pad the size of the element.
    size_t elementSize = m_Buffer.size() - elementOffset;
    elementSize = PadBufferToFourBytes(&m_Buffer, elementSize);

    // Go back and rewrite the element header
    assert(elementSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = elementSize;
    elementHeader.Type = hlsl::DxilShaderSourceElementType::Args;
    memcpy(m_Buffer.data() + elementOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }

  // Go back and rewrite the main header.
  assert(m_Buffer.size() >= sizeof(mainHeader));
  memcpy(m_Buffer.data(), &mainHeader, sizeof(mainHeader));

  size_t mainPartSize = m_Buffer.size();
  mainPartSize = PadBufferToFourBytes(&m_Buffer, mainPartSize);
}

llvm::StringRef hlsl::SourceInfoWriter::GetBuffer() {
  if (m_Buffer.size())
    return llvm::StringRef((char *)m_Buffer.data(), m_Buffer.size());
  return llvm::StringRef();
}

