//===- MachineFunctionInitalizer.h - machine function initializer ---------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MachineFunctionInitializer.h                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file declares an interface that allows custom machine function       //
// initialization.                                                           //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CODEGEN_MACHINEFUNCTIONINITIALIZER_H
#define LLVM_CODEGEN_MACHINEFUNCTIONINITIALIZER_H

namespace llvm {

class MachineFunction;

/// This interface provides a way to initialize machine functions after they are
/// created by the machine function analysis pass.
class MachineFunctionInitializer {
  virtual void anchor();

public:
  virtual ~MachineFunctionInitializer() {}

  /// Initialize the machine function.
  ///
  /// Return true if error occurred.
  virtual bool initializeMachineFunction(MachineFunction &MF) = 0;
};

} // end namespace llvm

#endif
