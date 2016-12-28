//===- CoverageViewOptions.h - Code coverage display options -------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CoverageViewOptions.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_COV_COVERAGEVIEWOPTIONS_H
#define LLVM_COV_COVERAGEVIEWOPTIONS_H

#include "RenderingSupport.h"

namespace llvm {

/// \brief The options for displaying the code coverage information.
struct CoverageViewOptions {
  bool Debug;
  bool Colors;
  bool ShowLineNumbers;
  bool ShowLineStats;
  bool ShowRegionMarkers;
  bool ShowLineStatsOrRegionMarkers;
  bool ShowExpandedRegions;
  bool ShowFunctionInstantiations;

  /// \brief Change the output's stream color if the colors are enabled.
  ColoredRawOstream colored_ostream(raw_ostream &OS,
                                    raw_ostream::Colors Color) const {
    return llvm::colored_ostream(OS, Color, Colors);
  }
};
}

#endif // LLVM_COV_COVERAGEVIEWOPTIONS_H
