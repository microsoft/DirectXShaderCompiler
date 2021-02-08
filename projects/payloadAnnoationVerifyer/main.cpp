
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// payloadAccessVerifier                                         //
// Copyright (C) Nvidia Corporation. All rights reserved.                    //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file implements inter-stage validation for Payload information.      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/DXIL/DxilSubobject.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilRuntimeReflection.h"
#include "llvm/Support/CommandLine.h"

// DxilRuntimeReflection implementation
#include "dxc/DxilContainer/DxilRuntimeReflection.inl"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <windows.h>
#include "llvm/Support/MSFileSystem.h"

using namespace llvm;

cl::opt<std::string> InputFile("f", cl::desc("Specify input dxil filename"), cl::value_desc("filename"));
cl::opt<std::string> InputVerificationFile("d", cl::desc("Specify input verification filename"), cl::value_desc("filename"));
cl::opt<bool> Verbose("v", cl::desc("Enables verbose prints"), cl::Hidden);


std::vector<std::vector<char>> partsData;
std::vector<std::pair<const void *, size_t>> parts;

bool isFirstRun = false;

static bool PayloadRDATIsEqual(const DxilLibraryDesc &a,
                               const DxilLibraryDesc &b) {
  // Check if new payloads are part of DxilLibraryDesc
  if (b.NumPayloads > a.NumPayloads)
    return false; // new payload found

  for (uint32_t i = 0; i != a.NumPayloads; ++i) {
    DxilPayloadTypeDesc &payloadA = a.pPayloads[i];
    for (uint32_t j = 0; j != b.NumPayloads; ++j) {
      DxilPayloadTypeDesc &payloadB = b.pPayloads[j];
      if (lstrcmpW(payloadA.TypeName, payloadB.TypeName) == 0) {
        break;
      }
      // new payload found
      return false;
    }
  }
  return true;
}

static void DiagnoseFailures(const DxilPayloadMismatchDetail &errorDetails,
                             const DxilLibraryDesc &gold,
                             const DxilLibraryDesc &toVerify) {
  // Check if equal named payloads exist, payloads with different names
  // are considered different types.

  // TODO: improve error reporting
 using ErrorType =  DxilPayloadMismatchDetail::ErrorType;
  switch(errorDetails.errorType) { 
      case ErrorType::PayloadCountMismatch: 
          std::wcerr << "The number of payloads mismatch between the two sets. "
                     << "Expected: "
                     << gold.NumPayloads 
                     << " Got: " 
                     << toVerify.NumPayloads
                     << std::endl;
          break;
      case ErrorType::IncompletePayloadMatch: 
          std::wcerr << "Not all payload types match." << std::endl;
          break;
      case ErrorType::FieldCountMismatch: 
          std::wcerr << "Payload "
                     << std::wstring(gold.pPayloads[errorDetails.payloadIdxB].TypeName)
                     << " has different number of fields. Expected: "
                     << gold.pPayloads[errorDetails.payloadIdxB].NumFields
                     << " Got: "
                     << toVerify.pPayloads[errorDetails.payloadIdxA].NumFields
                     << std::endl;
          break;
      case ErrorType::FieldMismatch: 
          auto GoldDesc = gold.pPayloads[errorDetails.payloadIdxB].pFields[errorDetails.fieldIdx];
          auto GotDesc = toVerify.pPayloads[errorDetails.payloadIdxA].pFields[errorDetails.fieldIdx];
          std::wstring goldType;
          std::wstring gotType;
          if (GoldDesc.StructTypeIndex != DXIL::IndependentPayloadFieldType) {
            auto goldTypeIndex = gold.pPayloads[errorDetails.payloadIdxB]
                                     .pFields[errorDetails.fieldIdx]
                                     .StructTypeIndex;
            auto gotTypeIndex = toVerify.pPayloads[errorDetails.payloadIdxA]
                                    .pFields[errorDetails.fieldIdx]
                                    .StructTypeIndex;
            goldType = gold.pPayloads[goldTypeIndex].TypeName;
            gotType = toVerify.pPayloads[gotTypeIndex].TypeName;
          }
          std::wcerr << "Field "<< errorDetails.fieldIdx <<" mismatched in payload "<< std::wstring(gold.pPayloads[errorDetails.payloadIdxB].TypeName) <<"!" << std::endl;
          std::wcerr << "Expected:" << std::endl
            << (goldType.empty() ? L" Type: " + std::to_wstring(GoldDesc.Type) : L" Type: " + goldType)
            << " Size: " << GoldDesc.Size
            << " PAQ: " << GoldDesc.PayloadAccessQualifiers
            << "\n";        
          std::wcerr << "Got:" << std::endl
            << (gotType.empty() ? L" Type: " + std::to_wstring(GotDesc.Type) : L" Type: " + gotType)
            << " Size: " << GotDesc.Size
            << " PAQ: " << GotDesc.PayloadAccessQualifiers
            << "\n";
          break;      
  }
}

enum class VerificationResult { Ok, Failure };

static void PrintDxilLibraryDesc(const DxilLibraryDesc &desc) {
  if (!Verbose)
      return;

  std::wcout << "DxilLibraryDesc contains " << desc.NumPayloads << " payloads:\n";

  for (unsigned i = 0; i < desc.NumPayloads; ++i) {
    auto PayloadDesc = desc.pPayloads[i];
    std::wcout << "Payload " << i << " " << std::wstring(PayloadDesc.TypeName)
               << " contains " << PayloadDesc.NumFields << " fields:\n";
    for (unsigned j = 0; j < PayloadDesc.NumFields; ++j) {
      auto FieldDesc = PayloadDesc.pFields[j];
      std::wcout << "\t"
                 << "Field " << j << " Type: " << FieldDesc.Type
                 << " Size: " << FieldDesc.Size
                 << " StructTypeIndex: " << FieldDesc.StructTypeIndex
                 << " PAQ: " << FieldDesc.PayloadAccessQualifiers
                 << "\n";
    }
  }
}


static VerificationResult VerifyPayloadMatches(_In_reads_bytes_(RDATSize)
                                                   const void *pRDATData,
                                               _In_ uint32_t RDATSize) {
  VerificationResult result = VerificationResult::Ok;

  RDAT::DxilRuntimeReflection *inputReflection =
      RDAT::CreateDxilRuntimeReflection();
  if (!inputReflection->InitFromRDAT(pRDATData, RDATSize)) {
    std::cerr << "Fatal: failed to initialze reflection data! Invalid DXIL "
                 "Container?\n";
    return VerificationResult::Failure;
  }

  if (isFirstRun) {
    // We verified that we can load the RDAT part of the container.
    return VerificationResult::Ok;
  }

  const DxilLibraryDesc &toVerify = inputReflection->GetLibraryReflection();
  PrintDxilLibraryDesc(toVerify);

  bool verificationSucceeded = true;
  for (auto &pair : parts) {
    RDAT::DxilRuntimeReflection *dumpReflection =
        RDAT::CreateDxilRuntimeReflection();
    if (!dumpReflection->InitFromRDAT(pair.first, pair.second)) {
      std::cerr << "Fatal: failed to initialze reflection data! Invalid DXIL "
                   "Container?\n";
      return VerificationResult::Failure;
    }
    const DxilLibraryDesc &gold = dumpReflection->GetLibraryReflection();
    PrintDxilLibraryDesc(gold);

    DxilPayloadMismatchDetail errorDetails;

    // Verify if payload definitions match.
    verificationSucceeded &= VerifyDxilPayloadDescMatches(gold, toVerify, &errorDetails);
    if (!verificationSucceeded) {
      result = VerificationResult::Failure;
      // Print an error to the user.
      DiagnoseFailures(errorDetails, gold, toVerify);
      break;
    }
  }

  return result;
}

static bool ValidateDxilRDAT(const DxilContainerHeader *pContainer,
                             uint32_t ContainerSize) {

  VerificationResult result = VerificationResult::Failure;

  for (auto it = begin(pContainer), itEnd = end(pContainer); it != itEnd;
       ++it) {
    const DxilPartHeader *pPart = *it;
    switch (pPart->PartFourCC) {
    // Only care about RDAT and skip all other parts.
    case DFCC_RuntimeData: {
      result = VerifyPayloadMatches(GetDxilPartData(pPart), pPart->PartSize);
      if (result == VerificationResult::Ok && isFirstRun) {
        parts.emplace_back(GetDxilPartData(pPart), pPart->PartSize);
      }
      break;
    }
    default:
      continue;
    }
  }

  return result != VerificationResult::Failure;
}

static void printUsage() {
  std::cout << "Usage examples:\n payloadAccessVerifier -f library1.dxil\t\t\t\t\tCreates a verification database which could be used to verify subsequent DXIL files.\n "
               "payloadAccessVerifer -f library.dxil -d verification.dump\t\tVerifies the DXIL module against the verification database.\n";
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printUsage();
    return 1;
  }

  cl::ParseCommandLineOptions(argc, argv);

  std::string inputName = InputFile;
  std::string dumpName  = InputVerificationFile;
  isFirstRun = dumpName.empty();

  std::ifstream input(inputName, std::ios::binary);
  std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(input), {});

  if (!isFirstRun) {
    // Load dump file.
    std::ifstream dump(dumpName, std::ios::binary);
    if (dump.peek() == std::ifstream::traits_type::eof()) {
      std::cout << "Warning: " << dumpName << " does not contain data!\n";
      std::cout << "         " <<"creating new verification file from input data!\n";
    } else {
      // Load the content of the dump file.
      do {
        int size = 0;
        dump >> size;
        if (dump.eof() || size == 0)
          break;
        partsData.emplace_back();
        auto &partBuffer = partsData.back();
        partBuffer.resize(size);
        dump.read(partBuffer.data(), size);
        parts.emplace_back(partBuffer.data(), size);
      } while (!dump.eof());
    }

    // There was a dump file with no RDAT parts.
    // Falling back to just create a new dump file.
    if (parts.empty())
      isFirstRun = true;
  }

  if (buffer.empty()) {
    std::cerr << "Input file not found!\n";
    return -1;
  }

  const DxilContainerHeader *pContainer =
      reinterpret_cast<DxilContainerHeader *>(buffer.data());
  const bool verificationSucceeded =
      ValidateDxilRDAT(pContainer, buffer.size());

  if (verificationSucceeded) {
    // Save dump file.
    if (dumpName.empty())
      dumpName = "verification.dump";
    std::ofstream output(dumpName, std::ios::binary);
    for (auto &pair : parts) {
      output << pair.second;
      output.write((const char *)pair.first, pair.second);
    }
    output.close();
    return 0;
  }

  return -1;
}