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

static bool ZlibDecompress(const void *pBuffer, size_t BufferSizeInBytes, Buffer *output) {

  struct Zlib_Stream {
    z_stream stream = {};
    Zlib_Stream() {
      inflateInit(&stream);
    }
    ~Zlib_Stream() {
      inflateEnd(&stream);
    }
  };

  Zlib_Stream streamStorage;
  z_stream *stream = &streamStorage.stream;

  constexpr size_t CHUNK_SIZE = 1024 * 16;

  Buffer &readBuffer = *output;

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

  stream->avail_in = BufferSizeInBytes;
  stream->next_in = (Byte *)pBuffer;

  int status = Z_OK;

  try {
    while (true) {
      size_t readOffset = readBuffer.size();
      readBuffer.resize(readOffset + CHUNK_SIZE);

      stream->avail_out = CHUNK_SIZE;
      stream->next_out = (unsigned char *)readBuffer.data() + readOffset;

      status = inflate(stream, Z_NO_FLUSH);
      switch (status) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          // Failed to zlib decompress buffer
          return false;
      }

      if (stream->avail_out != 0)
        readBuffer.resize(readBuffer.size() - stream->avail_out);

      if (stream->avail_out != 0 || status == Z_STREAM_END) {
        break;
      }
    }
  }
  catch (std::bad_alloc) {
    return false;
  }

  if (status != Z_STREAM_END) {
    // Failed to zlib decompress buffer (likely due to incomplete data)
    return false;
  }

  return true;
}

llvm::StringRef SourceInfoReader::GetArgs() const {
  return m_Args;
}
llvm::StringRef SourceInfoReader::GetDefines() const {
  return m_Defines;
}
SourceInfoReader::Source SourceInfoReader::GetSource(unsigned i) const {
  return m_Sources[i];
}
unsigned SourceInfoReader::GetSourcesCount() const {
  return m_Sources.size();
}
void SourceInfoReader::Read(const hlsl::DxilSourceInfo *SourceInfo) {
  const hlsl::DxilShaderSourceInfoElement *element = (hlsl::DxilShaderSourceInfoElement *)(SourceInfo+1);
  for (unsigned i = 0; i < SourceInfo->ElementCount; i++) {
    switch (element->Type) {
    case hlsl::DxilSourceInfoElementType::Defines:
    {
      auto header = (hlsl::DxilSourceInfo_Options *)&element[1];
      m_Defines = { (char *)(header+1), header->SizeInBytes };
    } break;
    case hlsl::DxilSourceInfoElementType::Args:
    {
      auto header = (hlsl::DxilSourceInfo_Options *)&element[1];
      m_Args = { (char *)(header+1), header->SizeInBytes };
    } break;
    case hlsl::DxilSourceInfoElementType::Sources:
    {
      auto header = (hlsl::DxilSourceInfo_Sources *)&element[1];
      const hlsl::DxilSourceInfo_SourcesElement *src = nullptr;
      if (header->CompressType == hlsl::DxilSourceInfo_SourcesCompressType::Zlib) {
        m_UncompressedSources.reserve(header->UncompressedSizeInBytes);
        bool bDecompressSucc = ZlibDecompress(header+1, header->SizeInBytes, &m_UncompressedSources);
        assert(bDecompressSucc);
        if (bDecompressSucc) {
          src = (hlsl::DxilSourceInfo_SourcesElement *)m_UncompressedSources.data();
        }
      }
      else {
        assert(header->UncompressedSizeInBytes == header->UncompressedSizeInBytes);
        src = (hlsl::DxilSourceInfo_SourcesElement *)(header+1);
      }

      assert(src);
      if (src) {
        for (unsigned i = 0; i < header->FileCount; i++) {
          const void *ptr = src+1;
          llvm::StringRef name    = { (char *)ptr,  src->NameSize };
          llvm::StringRef content = { (char *)ptr + src->NameSize+1, src->ContentSize, };

          m_Sources.push_back({ name, content });

          src = (hlsl::DxilSourceInfo_SourcesElement *)((uint8_t *)src + src->SizeInDwords*sizeof(uint32_t));
        }
      }

    } break;
    }
    element = (hlsl::DxilShaderSourceInfoElement *)((uint8_t *)element + element->SizeInDwords*sizeof(uint32_t));
  }
}


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
  hlsl::DxilSourceInfo_SourcesElement header = {};
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

  // The following block of code is the only part that can throw.
  try {
    for (;;) {
      const size_t oldSize = compressedData.size();
      compressedData.resize(compressedData.size() + CHUNK_SIZE);

      stream->next_out = (unsigned char *)(compressedData.data() + oldSize);
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
  }
  catch (std::bad_alloc) {
    return false;
  }

  if (Z_STREAM_END != status)
    return false;

  return true;
}

void SourceInfoWriter::Write(clang::CodeGenOptions &cgOpts, clang::SourceManager &srcMgr) {
  m_Buffer.clear();

  // Write an empty header first.
  hlsl::DxilSourceInfo mainHeader = {};
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
    hlsl::DxilSourceInfo_Sources header = {};
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
      header.CompressType = hlsl::DxilSourceInfo_SourcesCompressType::Zlib;
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
    elementHeader.Type = hlsl::DxilSourceInfoElementType::Sources;
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

    hlsl::DxilSourceInfo_Options header = {};

    const size_t headerOffset = m_Buffer.size();
    Append(&m_Buffer, &header, sizeof(header));

    for (std::string &def : cgOpts.HLSLDefines) {
      Append(&m_Buffer, def.data(), def.size());
      Append(&m_Buffer, 0); // Null terminator
    }
    Append(&m_Buffer, 0); // Double null terminator

    // Go back and rewrite the header now that we know the size
    header.SizeInBytes = m_Buffer.size() - headerOffset;
    memcpy(m_Buffer.data() + headerOffset, &header, sizeof(header));

    // Calculate and pad the size of the element.
    uint32_t unpaddedElementSize = m_Buffer.size() - elementOffset;
    uint32_t elementSize = PadBufferToFourBytes(&m_Buffer, unpaddedElementSize);

    // Go back and rewrite the element header
    assert(elementSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = elementSize / 4;
    elementHeader.Type = hlsl::DxilSourceInfoElementType::Defines;
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

    hlsl::DxilSourceInfo_Options header = {};

    const size_t headerOffset = m_Buffer.size();
    Append(&m_Buffer, &header, sizeof(header));

    for (std::string &arg : cgOpts.HLSLArguments) {
      Append(&m_Buffer, arg.data(), arg.size());
      Append(&m_Buffer, 0); // Null terminator
    }
    Append(&m_Buffer, 0); // Double null terminator

    // Go back and rewrite the header now that we know the size
    header.SizeInBytes = m_Buffer.size() - headerOffset;
    memcpy(m_Buffer.data() + headerOffset, &header, sizeof(header));

    // Calculate and pad the size of the element.
    size_t elementSize = m_Buffer.size() - elementOffset;
    elementSize = PadBufferToFourBytes(&m_Buffer, elementSize);

    // Go back and rewrite the element header
    assert(elementSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = elementSize / 4;
    elementHeader.Type = hlsl::DxilSourceInfoElementType::Args;
    memcpy(m_Buffer.data() + elementOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }

  // Go back and rewrite the main header.
  assert(m_Buffer.size() >= sizeof(mainHeader));

  size_t mainPartSize = m_Buffer.size();
  mainPartSize = PadBufferToFourBytes(&m_Buffer, mainPartSize);
  assert(mainPartSize % sizeof(uint32_t) == 0);
  mainHeader.SizeInDwords = mainPartSize / 4;

  memcpy(m_Buffer.data(), &mainHeader, sizeof(mainHeader));
}

const hlsl::DxilSourceInfo *hlsl::SourceInfoWriter::GetPart() const {
  if (!m_Buffer.size())
    return nullptr;
  assert(m_Buffer.size() >= sizeof(hlsl::DxilSourceInfo));
  const hlsl::DxilSourceInfo *ret = (hlsl::DxilSourceInfo *)m_Buffer.data();
  assert(ret->SizeInDwords * sizeof(uint32_t) == m_Buffer.size());
  return ret;
}

