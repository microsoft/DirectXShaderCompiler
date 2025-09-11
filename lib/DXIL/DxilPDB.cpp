///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPDB.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helpers to wrap debug information in a PDB container.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

//
// This file contains code that helps creating our special PDB format. PDB
// format contains streams at fixed locations. Outside of those fixed
// locations, unless they are listed in the stream hash table, there is be no
// way to know what the stream is. As far as normal PDB's are concerned, they
// dont' really exist.
//
// For our purposes, we always put our data in one stream at a fixed index
// defined below. The data is an ordinary DXIL container format, with parts
// that are relevant for debugging.
//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/raw_ostream.h"

#include "dxc/DXIL/DxilPDB.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/dxcapi.h"

using namespace llvm;

static const uint32_t kPdbStreamIndex =
    1; // This is the fixed stream index where the PDB stream header is
static const uint32_t kDataStreamIndex =
    5; // This is the fixed stream index where we will store our custom data.
static const uint32_t kMsfBlockSize = 512;

static_assert(sizeof(hlsl::pdb::MSF_SuperBlock) <= kMsfBlockSize,
              "MSF Block too small.");

// Calculate how many blocks are needed
static uint32_t CalculateNumBlocks(uint32_t BlockSize, uint32_t Size) {
  return (Size / BlockSize) + ((Size % BlockSize) ? 1 : 0);
}

static HRESULT ReadAllBytes(IStream *pStream, void *pDst, size_t uSize) {
  ULONG uBytesRead = 0;
  IFR(pStream->Read(pDst, uSize, &uBytesRead));
  if (uBytesRead != uSize)
    return E_FAIL;
  return S_OK;
}

struct MSFWriter {

  struct Stream {
    ArrayRef<char> Data;
    unsigned NumBlocks = 0;
  };
  struct StreamLayout {
    hlsl::pdb::MSF_SuperBlock SB;
  };

  int m_NumStreamBlocks = 0;
  SmallVector<Stream, 8> m_Streams;

  static uint32_t GetNumBlocks(uint32_t Size) {
    return CalculateNumBlocks(kMsfBlockSize, Size);
  }

  uint32_t AddStream(ArrayRef<char> Data) {
    uint32_t ID = m_Streams.size();
    Stream S;
    S.Data = Data;
    S.NumBlocks = GetNumBlocks(Data.size());
    m_NumStreamBlocks += S.NumBlocks;
    m_Streams.push_back(S);
    return ID;
  }

  uint32_t AddEmptyStream() { return AddStream({}); }

  uint32_t CalculateStreamDirectorySize() {
    uint32_t DirectorySizeInBytes = 0;
    DirectorySizeInBytes += sizeof(uint32_t); // NumStreams
    DirectorySizeInBytes +=
        m_Streams.size() *
        sizeof(uint32_t); // Stream sizes (in number of blocks)
    DirectorySizeInBytes +=
        m_NumStreamBlocks * sizeof(uint32_t); // Block indices for each stream
    return DirectorySizeInBytes;
  }

  struct BlockWriter {
    uint32_t BlocksWritten = 0;
    raw_ostream &OS;

    BlockWriter(raw_ostream &OS) : OS(OS) {}

    void WriteZeroPads(uint32_t Count) {
      for (unsigned i = 0; i < Count; i++)
        OS.write(0);
    }

    void WriteEmptyBlock() {
      BlocksWritten++;
      WriteZeroPads(kMsfBlockSize);
    }

    void WriteBlocks(uint32_t NumBlocks, const void *Data, uint32_t Size) {
      assert(NumBlocks >= GetNumBlocks(Size) &&
             "Cannot fit data into the requested number of blocks!");
      uint32_t TotalSize = NumBlocks * kMsfBlockSize;
      OS.write(static_cast<char *>(const_cast<void *>(Data)), Size);
      WriteZeroPads(TotalSize - Size);
      BlocksWritten += NumBlocks;
    }

    void WriteUint32(uint32_t Value) {
      support::ulittle32_t ValueLE;
      ValueLE = Value;
      OS.write((char *)&ValueLE, sizeof(ValueLE));
    }
  };

  void WriteBlocks(raw_ostream &OS, ArrayRef<char> Data, uint32_t NumBlocks) {
    assert(NumBlocks >= GetNumBlocks(Data.size()) &&
           "Cannot fit data into the requested number of blocks!");
    uint32_t TotalSize = NumBlocks * kMsfBlockSize;
    OS.write(Data.data(), Data.size());
    WriteZeroPadding(OS, TotalSize - Data.size());
  }
  void WriteZeroPadding(raw_ostream &OS, int Count) {
    for (int i = 0; i < Count; i++)
      OS.write(0);
  }

  static support::ulittle32_t MakeUint32LE(uint32_t Value) {
    support::ulittle32_t ValueLE;
    ValueLE = Value;
    return ValueLE;
  }

  // The starting block index of block addr. This skips over:
  //  [0] Superblock
  //  [1] FPM1
  //  [2] FPM2
  static constexpr uint32_t kBlockAddrStart = 3;

  void WriteToStream(raw_ostream &OS) {
    const uint32_t StreamDirectorySizeInBytes = CalculateStreamDirectorySize();
    const uint32_t StreamDirectoryNumBlocks =
        GetNumBlocks(StreamDirectorySizeInBytes);

    // The block addr is a list of uint32's pointing to a list of blocks
    // where the stream directory lives.
    const uint32_t BlockAddrSizeInBytes =
        StreamDirectoryNumBlocks * sizeof(support::ulittle32_t);

    // TODO: Dynamically adjust block size based on size.
    //
    // The block addr itself should be only one block. If we end up with
    // a stream directory so large that block addr won't fit in one block,
    // we would have to adjust the block size.
    //
    // However, with our block size as 512 bytes, we can only fit 128 uint32's
    // in there, each one points to one block for the stream directory would
    // mean 128 * 512 -> 65536 bytes -> 16384 uint32's for the stream directory.
    // Let's say only half of that size can be used to point stream blocks (in
    // reality most of them are used for stream blocks), that's 16384/2 * 512 ->
    // 4194304 -> ~4MB for streams.
    //
    // If we encounter sizes bigger than that, we would need to djust the block
    // size to fit the block addr into a single block.
    //
    const uint32_t BlockAddrNumBlocks = GetNumBlocks(BlockAddrSizeInBytes);

    const uint32_t StreamDirectoryStart = kBlockAddrStart + BlockAddrNumBlocks;
    const uint32_t StreamStart =
        StreamDirectoryStart + StreamDirectoryNumBlocks;

    hlsl::pdb::MSF_SuperBlock SB = {};
    {
      memcpy(SB.MagicBytes, hlsl::pdb::kMsfMagic, sizeof(hlsl::pdb::kMsfMagic));
      SB.BlockSize = kMsfBlockSize;
      SB.NumDirectoryBytes = StreamDirectorySizeInBytes;
      SB.NumBlocks = kBlockAddrStart /*super block + FPM1 + FPM2*/ +
                     m_NumStreamBlocks + StreamDirectoryNumBlocks +
                     BlockAddrNumBlocks;
      SB.FreeBlockMapBlock = 1;
      SB.BlockMapAddr = kBlockAddrStart;
    }

    BlockWriter Writer(OS);
    Writer.WriteBlocks(1, &SB, sizeof(SB)); // Super Block
    Writer.WriteEmptyBlock();               // FPM 1
    Writer.WriteEmptyBlock();               // FPM 2

    // BlockAddr
    // This block contains a list of uint32's that point to the blocks that
    // make up the stream directory. In the MSF spec, these blocks don't
    // necessarily have to be contiguous.
    {
      SmallVector<support::ulittle32_t, 4> BlockAddr;
      uint32_t Start = StreamDirectoryStart;
      for (unsigned i = 0; i < StreamDirectoryNumBlocks; i++) {
        support::ulittle32_t V;
        V = Start++;
        BlockAddr.push_back(V);
      }
      assert(BlockAddrSizeInBytes == sizeof(BlockAddr[0]) * BlockAddr.size());
      Writer.WriteBlocks(BlockAddrNumBlocks, BlockAddr.data(),
                         BlockAddr.size() * sizeof(BlockAddr[0]));
    }

    // Stream Directory. Describes where all the streams are
    // Looks like this:
    //
    {
      SmallVector<support::ulittle32_t, 32> StreamDirectoryData;
      StreamDirectoryData.push_back(MakeUint32LE(m_Streams.size()));
      for (unsigned i = 0; i < m_Streams.size(); i++) {
        StreamDirectoryData.push_back(MakeUint32LE(m_Streams[i].Data.size()));
      }
      uint32_t Start = StreamStart;
      for (unsigned i = 0; i < m_Streams.size(); i++) {
        auto &Stream = m_Streams[i];
        for (unsigned j = 0; j < Stream.NumBlocks; j++) {
          StreamDirectoryData.push_back(MakeUint32LE(Start++));
        }
      }
      Writer.WriteBlocks(StreamDirectoryNumBlocks, StreamDirectoryData.data(),
                         StreamDirectoryData.size() *
                             sizeof(StreamDirectoryData[0]));
    }

    // Write the streams.
    {
      for (unsigned i = 0; i < m_Streams.size(); i++) {
        auto &Stream = m_Streams[i];
        Writer.WriteBlocks(Stream.NumBlocks, Stream.Data.data(),
                           Stream.Data.size());
      }
    }
  }
};

enum class PdbStreamVersion : uint32_t {
  VC2 = 19941610,
  VC4 = 19950623,
  VC41 = 19950814,
  VC50 = 19960307,
  VC98 = 19970604,
  VC70Dep = 19990604,
  VC70 = 20000404,
  VC80 = 20030901,
  VC110 = 20091201,
  VC140 = 20140508,
};

struct PdbStreamHeader {
  support::ulittle32_t Version;
  support::ulittle32_t Signature;
  support::ulittle32_t Age;
  uint8_t UniqueId[16];
};
static_assert(sizeof(PdbStreamHeader) == 28, "PDB Header incorrect.");

static SmallVector<char, 0> WritePdbStream(ArrayRef<BYTE> Hash) {
  PdbStreamHeader Header = {};
  Header.Version = (uint32_t)PdbStreamVersion::VC70;
  Header.Age = 1;
  Header.Signature = 0;
  DXASSERT_NOMSG(Hash.size() == sizeof(Header.UniqueId));
  memcpy(Header.UniqueId, Hash.data(),
         std::min(Hash.size(), sizeof(Header.UniqueId)));

  SmallVector<char, 0> Result;
  raw_svector_ostream OS(Result);

  auto WriteU32 = [&](uint32_t val) {
    support::ulittle32_t valLE;
    valLE = val;
    OS.write((char *)&valLE, sizeof(valLE));
  };

  OS.write((char *)&Header, 28);
  WriteU32(0); // String buffer size
  WriteU32(0); // Size
  WriteU32(1); // Capacity // Capacity is required to be 1.
  WriteU32(0); // Present count
  WriteU32(0); // Deleted count

  WriteU32(0); // Key
  WriteU32(0); // Value

  OS.flush();
  return Result;
}

HRESULT hlsl::pdb::WriteDxilPDB(IMalloc *pMalloc, IDxcBlob *pContainer,
                                ArrayRef<BYTE> HashData, IDxcBlob **ppOutBlob) {
  return hlsl::pdb::WriteDxilPDB(
      pMalloc,
      llvm::ArrayRef<BYTE>((const BYTE *)pContainer->GetBufferPointer(),
                           pContainer->GetBufferSize()),
      HashData, ppOutBlob);
}

HRESULT hlsl::pdb::WriteDxilPDB(IMalloc *pMalloc,
                                llvm::ArrayRef<BYTE> ContainerData,
                                llvm::ArrayRef<BYTE> HashData,
                                IDxcBlob **ppOutBlob) {
  if (!hlsl::IsValidDxilContainer(
          (const hlsl::DxilContainerHeader *)ContainerData.data(),
          ContainerData.size()))
    return E_FAIL;

  SmallVector<char, 0> PdbStream = WritePdbStream(HashData);

  MSFWriter Writer;
  Writer.AddEmptyStream();     // Old Directory
  Writer.AddStream(PdbStream); // PDB Header

  // Fixed streams
  Writer.AddEmptyStream(); // TPI
  Writer.AddEmptyStream(); // DBI
  Writer.AddEmptyStream(); // IPI

  Writer.AddStream(
      llvm::ArrayRef<char>((const char *)ContainerData.data(),
                           ContainerData.size())); // Actual data block

  CComPtr<hlsl::AbstractMemoryStream> pStream;
  IFR(hlsl::CreateMemoryStream(pMalloc, &pStream));

  raw_stream_ostream OS(pStream);
  Writer.WriteToStream(OS);
  OS.flush();

  IFR(pStream.QueryInterface(ppOutBlob));

  return S_OK;
}

struct PDBReader {
  IStream *m_pStream = nullptr;
  IMalloc *m_pMalloc = nullptr;
  UINT32 m_uOriginalOffset = 0;
  hlsl::pdb::MSF_SuperBlock m_SB = {};
  HRESULT m_Status = S_OK;

  HRESULT SetPosition(INT32 sOffset) {
    LARGE_INTEGER Distance = {};
    Distance.QuadPart = m_uOriginalOffset + sOffset;
    ULARGE_INTEGER NewLocation = {};
    return m_pStream->Seek(Distance, STREAM_SEEK_SET, &NewLocation);
  }

  PDBReader(IMalloc *pMalloc, IStream *pStream)
      : m_pStream(pStream), m_pMalloc(pMalloc) {
    m_Status = ReadSuperblock(&m_SB);
  }

  // Reset the stream back to its original position, regardless of
  // we succeeded or failed.
  ~PDBReader() { SetPosition(0); }

  HRESULT GetStatus() { return m_Status; }

  HRESULT ReadSuperblock(hlsl::pdb::MSF_SuperBlock *pSB) {
    IFR(ReadAllBytes(m_pStream, pSB, sizeof(*pSB)));
    if (memcmp(pSB->MagicBytes, hlsl::pdb::kMsfMagic,
               sizeof(hlsl::pdb::kMsfMagic)) != 0)
      return E_FAIL;

    return S_OK;
  }

  HRESULT ReadU32(UINT32 *pValue) {
    support::ulittle32_t ValueLE;
    IFR(ReadAllBytes(m_pStream, &ValueLE, sizeof(ValueLE)));
    *pValue = ValueLE;
    return S_OK;
  }

  HRESULT GoToBeginningOfBlock(UINT32 uBlock) {
    return SetPosition(uBlock * m_SB.BlockSize);
  }

  HRESULT OffsetByU32(int sCount) {
    LARGE_INTEGER Offset = {};
    ULARGE_INTEGER BytesMoved = {};
    Offset.QuadPart = sCount * sizeof(UINT32);

    return m_pStream->Seek(Offset, STREAM_SEEK_CUR, &BytesMoved);
  }

  HRESULT ReadWholeStream(uint32_t StreamIndex, IDxcBlob **ppData) {
    if (FAILED(m_Status))
      return m_Status;

    UINT32 uNumDirectoryBlocks =
        CalculateNumBlocks(m_SB.BlockSize, m_SB.NumDirectoryBytes);

    // Load in the directory blocks
    llvm::SmallVector<uint32_t, 32> DirectoryBlocks;
    IFR(GoToBeginningOfBlock(m_SB.BlockMapAddr))
    for (unsigned i = 0; i < uNumDirectoryBlocks; i++) {
      UINT32 uBlock = 0;
      IFR(ReadU32(&uBlock));
      DirectoryBlocks.push_back(uBlock);
    }

    // Load Num streams
    UINT32 uNumStreams = 0;
    IFR(GoToBeginningOfBlock(DirectoryBlocks[0]));
    IFR(ReadU32(&uNumStreams));

    // If we don't have enough streams, then give up.
    if (uNumStreams <= StreamIndex)
      return E_FAIL;

    llvm::SmallVector<uint32_t, 6> StreamSizes;
    IFR(ReadU32ListFromBlocks(DirectoryBlocks, 1, uNumStreams, StreamSizes));

    UINT32 uOffsets = 0;
    for (unsigned i = 0; i < StreamIndex; i++) {
      UINT32 uNumBlocks = CalculateNumBlocks(m_SB.BlockSize, StreamSizes[i]);
      uOffsets += uNumBlocks;
    }

    llvm::SmallVector<uint32_t, 12> DataBlocks;
    IFR(ReadU32ListFromBlocks(
        DirectoryBlocks, 1 + uNumStreams + uOffsets,
        CalculateNumBlocks(m_SB.BlockSize, StreamSizes[StreamIndex]),
        DataBlocks));

    if (DataBlocks.size() == 0)
      return E_FAIL;

    IFR(GoToBeginningOfBlock(DataBlocks[0]));

    CComPtr<hlsl::AbstractMemoryStream> pResult;
    IFR(CreateMemoryStream(m_pMalloc, &pResult));

    std::vector<char> CopyBuffer;
    CopyBuffer.resize(m_SB.BlockSize);
    for (unsigned i = 0; i < DataBlocks.size(); i++) {
      IFR(GoToBeginningOfBlock(DataBlocks[i]));
      IFR(ReadAllBytes(m_pStream, CopyBuffer.data(), m_SB.BlockSize));
      ULONG uSizeWritten = 0;
      IFR(pResult->Write(CopyBuffer.data(), m_SB.BlockSize, &uSizeWritten));
      if (uSizeWritten != m_SB.BlockSize)
        return E_FAIL;
    }

    IFR(pResult.QueryInterface(ppData));

    return S_OK;
  }

  HRESULT ReadU32ListFromBlocks(ArrayRef<uint32_t> Blocks, UINT32 uOffsetByU32,
                                UINT32 uNumU32,
                                SmallVectorImpl<uint32_t> &Output) {
    if (Blocks.size() == 0)
      return E_FAIL;
    Output.clear();

    for (unsigned i = 0; i < uNumU32; i++) {
      UINT32 uOffsetInBytes = (uOffsetByU32 + i) * sizeof(UINT32);
      UINT32 BlockIndex = uOffsetInBytes / m_SB.BlockSize;
      UINT32 ByteOffset = uOffsetInBytes % m_SB.BlockSize;

      UINT32 uBlock = Blocks[BlockIndex];
      IFR(GoToBeginningOfBlock(uBlock));
      IFR(OffsetByU32(ByteOffset / sizeof(UINT32)));

      UINT32 uData = 0;
      IFR(ReadU32(&uData));

      Output.push_back(uData);
    }

    return S_OK;
  }
};

HRESULT hlsl::pdb::LoadDataFromStream(IMalloc *pMalloc, IStream *pIStream,
                                      IDxcBlob **ppHash,
                                      IDxcBlob **ppContainer) {
  PDBReader Reader(pMalloc, pIStream);

  if (ppHash) {
    CComPtr<IDxcBlob> pPdbStream;
    IFR(Reader.ReadWholeStream(kPdbStreamIndex, &pPdbStream));

    if (pPdbStream->GetBufferSize() < sizeof(PdbStreamHeader))
      return E_FAIL;

    PdbStreamHeader PdbHeader = {};
    memcpy(&PdbHeader, pPdbStream->GetBufferPointer(), sizeof(PdbHeader));

    CComPtr<hlsl::AbstractMemoryStream> pHash;
    IFR(CreateMemoryStream(pMalloc, &pHash));
    ULONG uBytesWritten = 0;
    IFR(pHash->Write(PdbHeader.UniqueId, sizeof(PdbHeader.UniqueId),
                     &uBytesWritten));

    if (uBytesWritten != sizeof(PdbHeader.UniqueId))
      return E_FAIL;

    IFR(pHash.QueryInterface(ppHash));
  }

  CComPtr<IDxcBlob> pContainer;
  IFR(Reader.ReadWholeStream(kDataStreamIndex, &pContainer));

  if (!hlsl::IsValidDxilContainer(
          (hlsl::DxilContainerHeader *)pContainer->GetBufferPointer(),
          pContainer->GetBufferSize()))
    return E_FAIL;

  *ppContainer = pContainer.Detach();

  return S_OK;
}

HRESULT hlsl::pdb::LoadDataFromStream(IMalloc *pMalloc, IStream *pIStream,
                                      IDxcBlob **ppContainer) {
  return LoadDataFromStream(pMalloc, pIStream, nullptr, ppContainer);
}
