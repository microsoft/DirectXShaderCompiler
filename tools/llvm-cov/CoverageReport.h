//===- CoverageReport.h - Code coverage report ---------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CoverageReport.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This class implements rendering of a code coverage report.                //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_COV_COVERAGEREPORT_H
#define LLVM_COV_COVERAGEREPORT_H

#include "CoverageSummaryInfo.h"
#include "CoverageViewOptions.h"

namespace llvm {

/// \brief Displays the code coverage report.
class CoverageReport {
  const CoverageViewOptions &Options;
  std::unique_ptr<coverage::CoverageMapping> Coverage;

  void render(const FileCoverageSummary &File, raw_ostream &OS);
  void render(const FunctionCoverageSummary &Function, raw_ostream &OS);

public:
  CoverageReport(const CoverageViewOptions &Options,
                 std::unique_ptr<coverage::CoverageMapping> Coverage)
      : Options(Options), Coverage(std::move(Coverage)) {}

  void renderFunctionReports(ArrayRef<std::string> Files, raw_ostream &OS);

  void renderFileReports(raw_ostream &OS);
};
}

#endif // LLVM_COV_COVERAGEREPORT_H
