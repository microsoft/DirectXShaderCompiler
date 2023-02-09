///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerAssembler.h                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helpers for writing to dxil container.                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/Global.h"        // |
#include "dxc/Support/Unicode.h"       // |
#include "dxc/Support/WinIncludes.h"   // |
#include "dxc/Support/FileIOHelper.h"  // | - to allow dxcapi.impl.h to be included
#include "dxc/Support/dxcapi.impl.h" // to identify the 'IDxcVersionInfo' identifier
#include "llvm/ADT/StringRef.h"

struct IStream;
class DxilPipelineStateValidation;

namespace llvm {
class Module;
}

namespace hlsl {

class AbstractMemoryStream;
class DxilModule;
class RootSignatureHandle;
class ShaderModel;
namespace DXIL {
enum class SignatureKind;
struct Part {
    typedef std::function<HRESULT(IStream *)> WriteProc;
    UINT32 uFourCC = 0;
    UINT32 uSize = 0;
    WriteProc Writer;

    Part(UINT32 uFourCC, UINT32 uSize, WriteProc Writer) :
      uFourCC(uFourCC),
      uSize(uSize),
      Writer(Writer)
    {}
  };
  
  struct CompilerVersionPartWriter {
    hlsl::DxilCompilerVersion m_Header = {};
    CComHeapPtr<char> m_CommitShaStorage;
    llvm::StringRef m_CommitSha = "";
    CComHeapPtr<char> m_CustomStringStorage;
    llvm::StringRef m_CustomString = "";

    void Init(IDxcVersionInfo *pVersionInfo) {
      m_Header = {};

      UINT32 Major = 0, Minor = 0;
      UINT32 Flags = 0;
      IFT(pVersionInfo->GetVersion(&Major, &Minor));
      IFT(pVersionInfo->GetFlags(&Flags));

      m_Header.Major = Major;
      m_Header.Minor = Minor;
      m_Header.VersionFlags = Flags;
      CComPtr<IDxcVersionInfo2> pVersionInfo2;
      if (SUCCEEDED(pVersionInfo->QueryInterface(&pVersionInfo2))) {
        UINT32 CommitCount = 0;
        IFT(pVersionInfo2->GetCommitInfo(&CommitCount, &m_CommitShaStorage));
        m_CommitSha = llvm::StringRef(m_CommitShaStorage.m_pData, strlen(m_CommitShaStorage.m_pData));
        m_Header.CommitCount = CommitCount;
        m_Header.VersionStringListSizeInBytes += m_CommitSha.size();
      }
      m_Header.VersionStringListSizeInBytes += /*null term*/ 1;

      CComPtr<IDxcVersionInfo3> pVersionInfo3;
      if (SUCCEEDED(pVersionInfo->QueryInterface(&pVersionInfo3))) {
        IFT(pVersionInfo3->GetCustomVersionString(&m_CustomStringStorage));
        m_CustomString = llvm::StringRef(m_CustomStringStorage, strlen(m_CustomStringStorage.m_pData));
        m_Header.VersionStringListSizeInBytes += m_CustomString.size();
      }
      m_Header.VersionStringListSizeInBytes += /*null term*/ 1;
    }

    static uint32_t PadToDword(uint32_t size, uint32_t *outNumPadding=nullptr) {
      uint32_t rem = size % 4;
      if (rem) {
        uint32_t padding = (4 - rem);
        if (outNumPadding)
          *outNumPadding = padding;
        return size + padding;
      }
      if (outNumPadding)
        *outNumPadding = 0;
      return size;
    }

    UINT32 GetSize(UINT32 *pPadding = nullptr) const {
      return PadToDword(sizeof(m_Header) + m_Header.VersionStringListSizeInBytes, pPadding);
    }

    void Write(IStream *pStream) {
      const uint8_t padByte = 0;
      UINT32 uPadding = 0;
      UINT32 uSize = GetSize(&uPadding);
      (void)uSize;

      ULONG cbWritten = 0;
      IFT(pStream->Write(&m_Header, sizeof(m_Header), &cbWritten));

      // Write a null terminator even if the string is empty
      IFT(pStream->Write(m_CommitSha.data(), m_CommitSha.size(), &cbWritten));
      // Null terminator for the commit sha
      IFT(pStream->Write(&padByte, sizeof(padByte), &cbWritten));

      // Write the custom version string.
      IFT(pStream->Write(m_CustomString.data(), m_CustomString.size(), &cbWritten));
      // Null terminator for the custom version string.
      IFT(pStream->Write(&padByte, sizeof(padByte), &cbWritten));

      // Write padding
      for (unsigned i = 0; i < uPadding; i++) {
        IFT(pStream->Write(&padByte, sizeof(padByte), &cbWritten));
      }
    }
  };

}

class DxilPartWriter {
public:
  virtual ~DxilPartWriter() {}
  virtual uint32_t size() const = 0;
  virtual void write(AbstractMemoryStream *pStream) = 0;
};

class DxilContainerWriter : public DxilPartWriter  {
public:
  typedef std::function<void(AbstractMemoryStream*)> WriteFn;
  virtual ~DxilContainerWriter() {}
  virtual void AddPart(uint32_t FourCC, uint32_t Size, WriteFn Write) = 0;
};

DxilPartWriter *NewProgramSignatureWriter(const DxilModule &M, DXIL::SignatureKind Kind);
DxilPartWriter *NewRootSignatureWriter(const RootSignatureHandle &S);
DxilPartWriter *NewFeatureInfoWriter(const DxilModule &M);
DxilPartWriter *NewPSVWriter(const DxilModule &M, uint32_t PSVVersion = UINT_MAX);
DxilPartWriter *NewRDATWriter(const DxilModule &M);

// Store serialized ViewID data from DxilModule to PipelineStateValidation.
void StoreViewIDStateToPSV(const uint32_t *pInputData,
                           unsigned InputSizeInUInts,
                           DxilPipelineStateValidation &PSV);
// Load ViewID state from PSV back to DxilModule view state vector.
// Pass nullptr for pOutputData to compute and return needed OutputSizeInUInts.
unsigned LoadViewIDStateFromPSV(unsigned *pOutputData,
                                unsigned OutputSizeInUInts,
                                const DxilPipelineStateValidation &PSV);

// Unaligned is for matching container for validator version < 1.7.
DxilContainerWriter *NewDxilContainerWriter(bool bUnaligned = false);

// Set validator version to 0,0 (not validated) then re-emit as much reflection metadata as possible.
void ReEmitLatestReflectionData(llvm::Module *pReflectionM);

// Strip functions and serialize module.
void StripAndCreateReflectionStream(llvm::Module *pReflectionM, uint32_t *pReflectionPartSizeInBytes, AbstractMemoryStream **ppReflectionStreamOut);

void WriteProgramPart(const hlsl::ShaderModel *pModel,
                      AbstractMemoryStream *pModuleBitcode,
                      IStream *pStream);

void SerializeDxilContainerForModule(
    hlsl::DxilModule *pModule, AbstractMemoryStream *pModuleBitcode,
    IDxcVersionInfo *DXCVersionInfo,
    AbstractMemoryStream *pStream, llvm::StringRef DebugName,
    SerializeDxilFlags Flags, DxilShaderHash *pShaderHashOut = nullptr,
    AbstractMemoryStream *pReflectionStreamOut = nullptr,
    AbstractMemoryStream *pRootSigStreamOut = nullptr,
    void *pPrivateData = nullptr,
    size_t PrivateDataSize = 0);
void SerializeDxilContainerForRootSignature(hlsl::RootSignatureHandle *pRootSigHandle,
                                     AbstractMemoryStream *pStream);

} // namespace hlsl