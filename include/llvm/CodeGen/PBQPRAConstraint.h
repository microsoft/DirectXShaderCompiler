//===-- RegAllocPBQP.h ------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PBQPRAConstraint.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines the PBQPBuilder interface, for classes which build PBQP //
// instances to represent register allocation problems, and the RegAllocPBQP //
// interface.                                                                //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CODEGEN_PBQPRACONSTRAINT_H
#define LLVM_CODEGEN_PBQPRACONSTRAINT_H

#include <memory>
#include <vector>

namespace llvm {
namespace PBQP {
namespace RegAlloc {
// Forward declare PBQP graph class.
class PBQPRAGraph;
}
}

class LiveIntervals;
class MachineBlockFrequencyInfo;
class MachineFunction;
class TargetRegisterInfo;

typedef PBQP::RegAlloc::PBQPRAGraph PBQPRAGraph;

/// @brief Abstract base for classes implementing PBQP register allocation
///        constraints (e.g. Spill-costs, interference, coalescing).
class PBQPRAConstraint {
public:
  virtual ~PBQPRAConstraint() = 0;
  virtual void apply(PBQPRAGraph &G) = 0;
private:
  virtual void anchor();
};

/// @brief PBQP register allocation constraint composer.
///
///   Constraints added to this list will be applied, in the order that they are
/// added, to the PBQP graph.
class PBQPRAConstraintList : public PBQPRAConstraint {
public:
  void apply(PBQPRAGraph &G) override {
    for (auto &C : Constraints)
      C->apply(G);
  }

  void addConstraint(std::unique_ptr<PBQPRAConstraint> C) {
    if (C)
      Constraints.push_back(std::move(C));
  }
private:
  std::vector<std::unique_ptr<PBQPRAConstraint>> Constraints;
  void anchor() override;
};

}

#endif /* LLVM_CODEGEN_PBQPRACONSTRAINT_H */
