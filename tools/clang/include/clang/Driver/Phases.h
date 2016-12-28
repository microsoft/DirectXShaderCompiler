//===--- Phases.h - Transformations on Driver Types -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Phases.h                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_DRIVER_PHASES_H
#define LLVM_CLANG_DRIVER_PHASES_H

namespace clang {
namespace driver {
namespace phases {
  /// ID - Ordered values for successive stages in the
  /// compilation process which interact with user options.
  enum ID {
    Preprocess,
    Precompile,
    Compile,
    Backend,
    Assemble,
    Link
  };

  enum {
    MaxNumberOfPhases = Link + 1
  };

  const char *getPhaseName(ID Id);

} // end namespace phases
} // end namespace driver
} // end namespace clang

#endif
