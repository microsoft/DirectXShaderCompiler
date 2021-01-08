#include "dxcshadersourceinfo.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "llvm/Support/Path.h"

using namespace hlsl;
using Buffer = SourceInfoWriter::Buffer;

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

static void AppendFileEntry(Buffer *buf, llvm::StringRef name, llvm::StringRef content) {
  hlsl::DxilShaderSourcesElement header = {};
  header.ContentSize = content.size();
  header.NameSize = name.size();
  header.SizeInDwords = PadToFourBytes(sizeof(header) + name.size()+1 + content.size()+1) / 4;

  Append(buf, &header, sizeof(header));
  Append(buf, name.data(), name.size());
  Append(buf, 0); // Null term
  Append(buf, content.data(), content.size());
  Append(buf, 0); // Null term
}

llvm::StringRef hlsl::SourceInfoWriter::GetBuffer() {
  return llvm::StringRef((char *)m_Buffer.data(), m_Buffer.size());
}

static uint32_t PadBufferToFourBytes(Buffer *buf, uint32_t unpaddedSize) {
  const uint32_t paddedSize = PadToFourBytes(unpaddedSize);
  // Padding.
  for (uint32_t i = unpaddedSize; i < paddedSize; i++) {
    Append(buf, 0);
  }
  return paddedSize;
}

void SourceInfoWriter::Write(clang::CodeGenOptions &CodeGenOpts, clang::SourceManager &srcMgr) {
  m_Buffer.clear();

  // Write an empty header first.
  hlsl::DxilShaderSourceInfo mainHeader = {};
  Append(&m_Buffer, &mainHeader, sizeof(mainHeader));

  ////////////////////////////////////////////////////////////////////
  // Add all file contents in a list of filename/content pairs.
  ////////////////////////////////////////////////////////////////////
  {
    size_t partOffset = 0;
    size_t partSize = 0;

    Buffer uncompressedBuffer;

    std::map<std::string, llvm::StringRef> filesMap;
    bool bFoundMainFile = false;
    for (auto it = srcMgr.fileinfo_begin(), end = srcMgr.fileinfo_end(); it != end; ++it) {
      if (it->first->isValid() && !it->second->IsSystemFile) {
        // If main file, write that to metadata first.
        // Add the rest to filesMap to sort by name.
        llvm::SmallString<128> NormalizedPath;
        llvm::sys::path::native(it->first->getName(), NormalizedPath);
        if (CodeGenOpts.MainFileName.compare(it->first->getName()) == 0) {
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

    hlsl::DxilShaderSourceInfoElement elementHeader = {};

    // @TODO: Compress
    hlsl::DxilShaderSources header = {};
    header.UncompressedSizeInBytes = uncompressedBuffer.size();
    header.SizeInBytes = uncompressedBuffer.size();
    header.FileCount = filesMap.size() + 1;

    partOffset = m_Buffer.size();
    Append(&m_Buffer, &elementHeader, sizeof(elementHeader)); // Write an empty header
    Append(&m_Buffer, &header, sizeof(header));
    Append(&m_Buffer, uncompressedBuffer.data(), uncompressedBuffer.size());
    uint32_t unpaddedPartSize = m_Buffer.size() - partOffset;
    partSize = PadBufferToFourBytes(&m_Buffer, unpaddedPartSize);

    // Go back and rewrite the element header
    assert(partSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = partSize / 4;
    elementHeader.Type = hlsl::DxilShaderSourceElementType::Sources;
    memcpy(m_Buffer.data() + partOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }

  ////////////////////////////////////////////////////////////////////
  // Defines
  ////////////////////////////////////////////////////////////////////
  {
    size_t partOffset = 0;
    size_t partSize = 0;

    partOffset = m_Buffer.size();

    hlsl::DxilShaderSourceInfoElement elementHeader = {};
    Append(&m_Buffer, &elementHeader, sizeof(elementHeader)); // Write an empty header

    hlsl::DxilShaderCompileOptions header = {};
    header.Count = CodeGenOpts.HLSLDefines.size();

    Append(&m_Buffer, &header, sizeof(header));

    for (std::string &def : CodeGenOpts.HLSLDefines) {
      Append(&m_Buffer, def.data(), def.size());
      Append(&m_Buffer, 0); // Null terminator
    }
    Append(&m_Buffer, 0); // Double null terminator

    uint32_t unpaddedPartSize = m_Buffer.size() - partOffset;
    partSize = PadBufferToFourBytes(&m_Buffer, unpaddedPartSize);

    // Go back and rewrite the element header
    assert(partSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = partSize;
    elementHeader.Type = hlsl::DxilShaderSourceElementType::Defines;
    memcpy(m_Buffer.data() + partOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }


  ////////////////////////////////////////////////////////////////////
  // Args
  ////////////////////////////////////////////////////////////////////
  {
    size_t partOffset = 0;

    hlsl::DxilShaderSourceInfoElement elementHeader = {};
    Append(&m_Buffer, &elementHeader, sizeof(elementHeader)); // Write an empty header

    hlsl::DxilShaderCompileOptions header = {};
    header.Count = CodeGenOpts.HLSLArguments.size();

    partOffset = m_Buffer.size();
    Append(&m_Buffer, &header, sizeof(header));

    for (std::string &arg : CodeGenOpts.HLSLArguments) {
      Append(&m_Buffer, arg.data(), arg.size());
      Append(&m_Buffer, 0); // Null terminator
    }
    Append(&m_Buffer, 0); // Double null terminator

    size_t partSize = m_Buffer.size() - partOffset;
    partSize = PadBufferToFourBytes(&m_Buffer, partSize);

    // Go back and rewrite the element header
    assert(partSize % sizeof(uint32_t) == 0);
    elementHeader.SizeInDwords = partSize;
    elementHeader.Type = hlsl::DxilShaderSourceElementType::Args;
    memcpy(m_Buffer.data() + partOffset, &elementHeader, sizeof(elementHeader));
    mainHeader.ElementCount++;
  }

  // Go back and rewrite the main header.
  assert(m_Buffer.size() >= sizeof(mainHeader));
  memcpy(m_Buffer.data(), &mainHeader, sizeof(mainHeader));

  size_t mainPartSize = m_Buffer.size();
  mainPartSize = PadBufferToFourBytes(&m_Buffer, mainPartSize);
}

