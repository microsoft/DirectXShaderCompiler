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

static bool ZlibDecompress(const void *pBuffer, size_t BufferSizeInBytes, Buffer *output);
static bool ZlibCompress(const void *src, size_t srcSize, Buffer *outCompressedData);


///////////////////////////////////////////////////////////////////////////////
// Reader
///////////////////////////////////////////////////////////////////////////////

SourceInfoReader::Source SourceInfoReader::GetSource(unsigned i) const {
  return m_Sources[i];
}
unsigned SourceInfoReader::GetSourcesCount() const {
  return m_Sources.size();
}
void SourceInfoReader::Read(const hlsl::DxilSourceInfo *SourceInfo) {
  const hlsl::DxilSourceInfoSection *section = (const hlsl::DxilSourceInfoSection *)(SourceInfo+1);

  for (unsigned i = 0; i < SourceInfo->SectionCount; i++) {
    switch (section->Type) {
    case hlsl::DxilSourceInfoSectionType::Args:
    {
      const hlsl::DxilSourceInfo_Args *header = (const hlsl::DxilSourceInfo_Args *)&section[1];
      const char *args = (const char *)(header + 1);
      int begin = 0;
      for (uint32_t i = 0; i < header->SizeInBytes && m_Args.size() < header->Count; i++) {
        assert(i < header->SizeInBytes);
        if (i >= header->SizeInBytes)
          break;
        if (args[i] == '\0') {
          uint32_t length = i - begin;
          m_Args.push_back(std::string(args+begin, args+begin+length));
          begin = i+1;
        }
      }
    } break;

    case hlsl::DxilSourceInfoSectionType::SourceNames:
    {
      const hlsl::DxilSourceInfo_SourceNames *header = (const hlsl::DxilSourceInfo_SourceNames *)&section[1];
      const hlsl::DxilSourceInfo_SourceNamesEntry *entry = (const hlsl::DxilSourceInfo_SourceNamesEntry *)(header+1);

      assert(m_Sources.size() == 0 || m_Sources.size() == header->Count);
      m_Sources.resize(header->Count);
      const hlsl::DxilSourceInfo_SourceNamesEntry *firstEntry = entry;
      for (unsigned i = 0; i < header->Count; i++) {
        const size_t endOffset = (const uint8_t *)entry - (const uint8_t *)firstEntry + entry->AlignedSizeInBytes;
        assert(endOffset <= header->EntriesSizeInBytes);
        if (endOffset > header->EntriesSizeInBytes)
          break;

        const void *ptr = entry+1;
        if (entry->NameSizeInBytes > 0) {
          llvm::StringRef name = { (const char *)ptr, entry->NameSizeInBytes-1, };
          m_Sources[i].Name = name;
        }

        entry = (const hlsl::DxilSourceInfo_SourceNamesEntry *)((const uint8_t *)entry + entry->AlignedSizeInBytes);
      }

    } break;
    case hlsl::DxilSourceInfoSectionType::SourceContents:
    {
      const hlsl::DxilSourceInfo_SourceContents *header = (const hlsl::DxilSourceInfo_SourceContents *)&section[1];
      const hlsl::DxilSourceInfo_SourceContentsEntry *entry = nullptr;
      if (header->CompressType == hlsl::DxilSourceInfo_SourceContentsCompressType::Zlib) {
        m_UncompressedSources.reserve(header->UncompressedEntriesSizeInBytes);
        bool bDecompressSucc = ZlibDecompress(header+1, header->EntriesSizeInBytes, &m_UncompressedSources);
        assert(bDecompressSucc);
        if (bDecompressSucc) {
          entry = (const hlsl::DxilSourceInfo_SourceContentsEntry *)m_UncompressedSources.data();
        }
      }
      else {
        assert(header->EntriesSizeInBytes == header->UncompressedEntriesSizeInBytes);
        entry = (const hlsl::DxilSourceInfo_SourceContentsEntry *)(header+1);
      }

      assert(entry);
      if (entry) {
        assert(m_Sources.size() == 0 || m_Sources.size() == header->Count);
        m_Sources.resize(header->Count);

        const hlsl::DxilSourceInfo_SourceContentsEntry *firstEntry = entry;
        for (unsigned i = 0; i < header->Count; i++) {
          const size_t endOffset = (const uint8_t *)entry - (const uint8_t *)firstEntry + entry->AlignedSizeInBytes;
          assert(endOffset <= header->UncompressedEntriesSizeInBytes);
          if (endOffset > header->UncompressedEntriesSizeInBytes)
            break;

          const void *ptr = entry+1;
          if (entry->ContentSizeInBytes > 0) {
            llvm::StringRef content = { (const char *)ptr, entry->ContentSizeInBytes-1, };
            m_Sources[i].Content = content;
          }

          entry = (const hlsl::DxilSourceInfo_SourceContentsEntry *)((const uint8_t *)entry + entry->AlignedSizeInBytes);
        }
      }

    } break;
    }
    section = (const hlsl::DxilSourceInfoSection *)((const uint8_t *)section + section->AlignedSizeInBytes);
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

static void AppendFileContentEntry(Buffer *buf, llvm::StringRef content) {
  hlsl::DxilSourceInfo_SourceContentsEntry header = {};
  header.AlignedSizeInBytes = PadToFourBytes(sizeof(header) + content.size()+1);
  header.ContentSizeInBytes = content.size()+1;

  const size_t offset = buf->size();
  Append(buf, &header, sizeof(header));
  Append(buf, content.data(), content.size());
  Append(buf, 0); // Null term

  const size_t paddedOffset = PadBufferToFourBytes(buf, buf->size() - offset);
  (void)paddedOffset;
  assert(paddedOffset == header.AlignedSizeInBytes);
}

static size_t BeginSection(Buffer *buf) {
  const size_t sectionOffset = buf->size();

  hlsl::DxilSourceInfoSection sectionHeader = {};
  Append(buf, &sectionHeader, sizeof(sectionHeader)); // Write an empty header

  return sectionOffset;
}

static void FinishSection(Buffer *buf, const size_t sectionOffset, hlsl::DxilSourceInfoSectionType type) {
  hlsl::DxilSourceInfoSection sectionHeader = {};

  // Calculate and pad the size of the section.
  const size_t sectionSize = buf->size() - sectionOffset;
  const size_t paddedSectionSize = PadBufferToFourBytes(buf, sectionSize);

  // Go back and rewrite the section header
  assert(paddedSectionSize % sizeof(uint32_t) == 0);
  sectionHeader.AlignedSizeInBytes = paddedSectionSize;
  sectionHeader.Type = type;
  memcpy(buf->data() + sectionOffset, &sectionHeader, sizeof(sectionHeader));
}

struct SourceFile {
  std::string Name;
  llvm::StringRef Content;
};

static std::vector<SourceFile> ComputeFileList(clang::CodeGenOptions &cgOpts, clang::SourceManager &srcMgr) {
  std::vector<SourceFile> ret;
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
          SourceFile file;
          file.Name = NormalizedPath.str();
          file.Content = it->second->getRawBuffer()->getBuffer();
          ret.push_back(file);
          assert(!bFoundMainFile && "otherwise, more than one file matches main filename");
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
      SourceFile file;
      file.Name = it.first;
      file.Content = it.second;
      ret.push_back(file);
    }
  }
  return ret;
}

void SourceInfoWriter::Write(llvm::StringRef targetProfile, llvm::StringRef entryPoint, clang::CodeGenOptions &cgOpts, clang::SourceManager &srcMgr) {
  m_Buffer.clear();

  // Write an empty header first.
  hlsl::DxilSourceInfo mainHeader = {};
  Append(&m_Buffer, &mainHeader, sizeof(mainHeader));

  std::vector<SourceFile> sourceFileList = ComputeFileList(cgOpts, srcMgr);

  ////////////////////////////////////////////////////////////////////
  // Add all file names in a list.
  ////////////////////////////////////////////////////////////////////
  {
    const size_t sectionOffset = BeginSection(&m_Buffer);

    // Write an empty header
    const size_t headerOffset = m_Buffer.size();
    hlsl::DxilSourceInfo_SourceNames header = {};
    Append(&m_Buffer, &header, sizeof(header));

    // Write the entries data
    const size_t dataOffset = m_Buffer.size();
    for (unsigned i = 0; i < sourceFileList.size(); i++) {
      SourceFile &file = sourceFileList[i];
      const size_t entryOffset = m_Buffer.size();

      // Write the header
      hlsl::DxilSourceInfo_SourceNamesEntry entryHeader = {};
      entryHeader.NameSizeInBytes = file.Name.size()+1;
      entryHeader.ContentSizeInBytes = file.Content.size()+1;
      entryHeader.AlignedSizeInBytes = PadToFourBytes(sizeof(entryHeader) + file.Name.size() + 1);

      Append(&m_Buffer, &entryHeader, sizeof(entryHeader));
      Append(&m_Buffer, file.Name.data(), file.Name.size());
      Append(&m_Buffer, 0); // Null terminator

      const size_t paddedOffset = PadBufferToFourBytes(&m_Buffer, m_Buffer.size() - entryOffset);
      (void)paddedOffset;
      assert(paddedOffset == entryHeader.AlignedSizeInBytes);
    }

    // Go back and write the header.
    header.Count = sourceFileList.size();
    header.EntriesSizeInBytes = m_Buffer.size() - dataOffset;
    memcpy(m_Buffer.data() + headerOffset, &header, sizeof(header));

    FinishSection(&m_Buffer, sectionOffset, hlsl::DxilSourceInfoSectionType::SourceNames);
    mainHeader.SectionCount++;
  }

  ////////////////////////////////////////////////////////////////////
  // Add all file contents in a list.
  ////////////////////////////////////////////////////////////////////
  {
    const size_t sectionOffset = BeginSection(&m_Buffer);

    // Put all the contents in a buffer
    Buffer uncompressedBuffer;
    const size_t contentOffset = m_Buffer.size();
    for (unsigned i = 0; i < sourceFileList.size(); i++) {
      SourceFile &file = sourceFileList[i];
      AppendFileContentEntry(&uncompressedBuffer, file.Content);
    }

    const size_t headerOffset = m_Buffer.size();

    // Write the header
    hlsl::DxilSourceInfo_SourceContents header = {};
    header.EntriesSizeInBytes = uncompressedBuffer.size();
    header.UncompressedEntriesSizeInBytes = uncompressedBuffer.size();
    header.Count = sourceFileList.size();
    Append(&m_Buffer, &header, sizeof(header));

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
      header.EntriesSizeInBytes = compressedSize;
      header.CompressType = hlsl::DxilSourceInfo_SourceContentsCompressType::Zlib;
      memcpy(m_Buffer.data() + headerOffset, &header, sizeof(header));
    }
    // Otherwise, just write the whole uncompressed
    else {
      Append(&m_Buffer, uncompressedBuffer.data(), uncompressedBuffer.size());
    }

    FinishSection(&m_Buffer, sectionOffset, hlsl::DxilSourceInfoSectionType::SourceContents);
    mainHeader.SectionCount++;
  }

  ////////////////////////////////////////////////////////////////////
  // Args
  ////////////////////////////////////////////////////////////////////
  {
    const size_t sectionOffset = BeginSection(&m_Buffer);

    hlsl::DxilSourceInfo_Args header = {};

    const size_t headerOffset = m_Buffer.size();
    Append(&m_Buffer, &header, sizeof(header));

    const size_t contentOffset = m_Buffer.size();
    uint32_t count = 0;
    for (std::string &arg : cgOpts.HLSLArguments) {
      Append(&m_Buffer, arg.data(), arg.size());
      Append(&m_Buffer, 0); // Null terminator
      count++;
    }

    // Go back and rewrite the header now that we know the size
    header.SizeInBytes = m_Buffer.size() - contentOffset;
    header.Count = count;
    memcpy(m_Buffer.data() + headerOffset, &header, sizeof(header));

    FinishSection(&m_Buffer, sectionOffset, hlsl::DxilSourceInfoSectionType::Args);
    mainHeader.SectionCount++;
  }

  // Go back and rewrite the main header.
  assert(m_Buffer.size() >= sizeof(mainHeader));

  size_t mainPartSize = m_Buffer.size();
  mainPartSize = PadBufferToFourBytes(&m_Buffer, mainPartSize);
  assert(mainPartSize % sizeof(uint32_t) == 0);
  mainHeader.AlignedSizeInBytes = mainPartSize;

  memcpy(m_Buffer.data(), &mainHeader, sizeof(mainHeader));
}

const hlsl::DxilSourceInfo *hlsl::SourceInfoWriter::GetPart() const {
  if (!m_Buffer.size())
    return nullptr;
  assert(m_Buffer.size() >= sizeof(hlsl::DxilSourceInfo));
  const hlsl::DxilSourceInfo *ret = (const hlsl::DxilSourceInfo *)m_Buffer.data();
  assert(ret->AlignedSizeInBytes == m_Buffer.size());
  return ret;
}





///////////////////////////////////////////////////////////////////////////////
// Compression helpers
///////////////////////////////////////////////////////////////////////////////

static void *ZlibMalloc(void *opaque, size_t items, size_t size) {
  void *ret = nullptr;
  try {
    ret = new uint8_t[items * size];
  }
  catch (std::bad_alloc e) {
    return NULL;
  }
  return ret;
}

static void ZlibFree(void *opaque, void *address) {
  delete[] (uint8_t *)address;
}

static bool ZlibDecompress(const void *pBuffer, size_t BufferSizeInBytes, Buffer *output) {

  struct Zlib_Stream {
    z_stream stream = {};
    ~Zlib_Stream() {
      inflateEnd(&stream);
    }
  };

  Zlib_Stream streamStorage;
  z_stream *stream = &streamStorage.stream;

  stream->zalloc = ZlibMalloc;
  stream->zfree = ZlibFree;
  inflateInit(stream);

  constexpr size_t CHUNK_SIZE = 1024 * 16;

  Buffer &readBuffer = *output;

  stream->avail_in = BufferSizeInBytes;
  stream->next_in = (const Byte *)pBuffer;

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

  stream->zalloc = ZlibMalloc;
  stream->zfree = ZlibFree;
  status = deflateInit(stream, level);
  if (Z_OK != status)
    return false;

  stream->next_in = (const unsigned char *)src;
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
