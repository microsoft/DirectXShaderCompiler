//===-- DWARFDebugFrame.h - Parsing of .debug_frame -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DWARFDebugFrame.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_DEBUGINFO_DWARFDEBUGFRAME_H
#define LLVM_LIB_DEBUGINFO_DWARFDEBUGFRAME_H

#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <vector>

namespace llvm {

class FrameEntry;

/// \brief A parsed .debug_frame section
///
class DWARFDebugFrame {
public:
  DWARFDebugFrame();
  ~DWARFDebugFrame();

  /// \brief Dump the section data into the given stream.
  void dump(raw_ostream &OS) const;

  /// \brief Parse the section from raw data.
  /// data is assumed to be pointing to the beginning of the section.
  void parse(DataExtractor Data);

private:
  std::vector<std::unique_ptr<FrameEntry>> Entries;
};


} // namespace llvm

#endif
