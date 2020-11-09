
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

// DxilRuntimeReflection implementation
#include "dxc/DxilContainer/DxilRuntimeReflection.inl"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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

static void DiagnoseFailures(const DxilLibraryDesc &a,
                             const DxilLibraryDesc &b) {
  // Check if equal named payloads exist, payloads with different names
  // are considered different types.
  for (uint32_t i = 0; i != a.NumPayloads; ++i) {
    DxilPayloadTypeDesc &payloadA = a.pPayloads[i];

    for (uint32_t j = 0; j != b.NumPayloads; ++j) {
      DxilPayloadTypeDesc &payloadB = b.pPayloads[j];
      if (lstrcmpW(payloadA.TypeName, payloadB.TypeName) == 0) {
        // Matching type names, these two payloads need deep verification.
        // For each payload pair that has equal names, check that fields match
        // and payload annotations are uniform in both types.
        if (payloadA.NumFields != payloadB.NumFields) {
          std::wcerr << "Error: number of payload fields mismatch for type: '"
                    << payloadA.TypeName << "' which has "
                    << payloadA.NumFields << " but " << payloadB.NumFields
                    << " were expected\n";
        }

        for (uint32_t k = 0; k != payloadA.NumFields; ++k) {
          DxilPayloadFieldDesc &fieldA = payloadA.pFields[k];
          DxilPayloadFieldDesc &fieldB = payloadB.pFields[k];

          if (k >= payloadB.NumFields || lstrcmpW(fieldA.FieldName, fieldB.FieldName) != 0) {
              std::wcerr << "Error: unexpected field '" << fieldA.FieldName << "'\n";
            continue;
          }

          if (fieldA.Type == fieldB.Type && fieldA.Size == fieldB.Size &&
              fieldA.PayloadAccessQualifier == fieldB.PayloadAccessQualifier)
            continue;

          if (fieldA.Type != fieldB.Type) {
            std::wcerr << "Error: type mismatch for field '" << fieldA.FieldName
                      << "'\n";
          }          
          if (fieldA.Size != fieldB.Size) {
            std::wcerr << "Error: size mismatch for field '" << fieldA.FieldName
                      << "'\n";
          }
          if (fieldA.PayloadAccessQualifier != fieldB.PayloadAccessQualifier) {
            std::wcerr << "Error: payload access qualifiers mismatch for field '"
                      << fieldA.FieldName << "'\n";
          }
        }

        break;
      }
    }
  }
}

enum class VerificationResult { Ok, Failur, OkNeedsAppend };

static VerificationResult VerifyPayloadMatches(_In_reads_bytes_(RDATSize)
                                                   const void *pRDATData,
                                               _In_ uint32_t RDATSize) {
  VerificationResult result = VerificationResult::Ok;

  RDAT::DxilRuntimeReflection *inputReflection =
      RDAT::CreateDxilRuntimeReflection();
  if (!inputReflection->InitFromRDAT(pRDATData, RDATSize)) {
    std::cerr << "Fatal: failed to initialze reflection data! Invalid DXIL "
                 "Container?\n";
    return VerificationResult::Failur;
  }

  if (isFirstRun) {
    return VerificationResult::OkNeedsAppend;
  }

  const DxilLibraryDesc &descA = inputReflection->GetLibraryReflection();
  bool verificationSucceeded = true;
  for (auto &pair : parts) {
    RDAT::DxilRuntimeReflection *dumpReflection =
        RDAT::CreateDxilRuntimeReflection();
    if (!dumpReflection->InitFromRDAT(pair.first, pair.second)) {
      std::cerr << "Fatal: failed to initialze reflection data! Invalid DXIL "
                   "Container?\n";
      return VerificationResult::Failur;
    }
    const DxilLibraryDesc &descB = dumpReflection->GetLibraryReflection();

    // Verify if payload definitions match.
    verificationSucceeded &= VerifyDxilPayloadDescMatches(descA, descB);
    if (!verificationSucceeded) {
      result = VerificationResult::Failur;
      // Print an error to the user.
      DiagnoseFailures(descA, descB);
      break;
    } else {
      // Payload structs are matching. Check if we need to
      // append the RDAT for next verifcation to the dump.
      if (!PayloadRDATIsEqual(descA, descB)) {
        result = VerificationResult::OkNeedsAppend;
      }
    }
  }

  return result;
}

static bool ValidateDxilRDAT(const DxilContainerHeader *pContainer,
                             uint32_t ContainerSize) {

  VerificationResult result = VerificationResult::Failur;

  for (auto it = begin(pContainer), itEnd = end(pContainer); it != itEnd;
       ++it) {
    const DxilPartHeader *pPart = *it;
    switch (pPart->PartFourCC) {
    // Only care about RDAT and skip all other parts.
    case DFCC_RuntimeData: {

      result = VerifyPayloadMatches(GetDxilPartData(pPart), pPart->PartSize);
      if (result == VerificationResult::OkNeedsAppend) {
        // If validation has succeeded append the RDAT part to the dump file
        // for next invocation.
        parts.emplace_back(GetDxilPartData(pPart), pPart->PartSize);
      }
      break;
    }
    default:
      continue;
    }
  }

  return result != VerificationResult::Failur;
}

static void printUsage() {
  std::cout << "Usage examples:\n payloadAccessVerifier -f library.dxil\n "
               "payloadAccessVerifer -f library.dxil -d verification.dump\n";
}

int main(int argc, char *argv[]) {

  if (argc < 3) {
    printUsage();
    return 1;
  }

  std::string inputName;
  std::string dumpName;

  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "-f") {
      if (i + 1 >= argc) {
        printUsage();
        return 2;
      }
      inputName = argv[i + 1];
      ++i;
      continue;
    }
    if (std::string(argv[i]) == "-d") {
      if (i + 1 >= argc) {
        printUsage();
        return 2;
      }
      dumpName = argv[i + 1];
      ++i;
      continue;
    }
  }

  isFirstRun = dumpName.empty();

  std::ifstream input(inputName, std::ios::binary);
  std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(input), {});

  if (!isFirstRun) {
    // Load dump file.
    std::ifstream dump(dumpName, std::ios::binary);
    if (dump.peek() == std::ifstream::traits_type::eof()) {
      std::cout << "Warning: " << dumpName << " does not contain data!\n";
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