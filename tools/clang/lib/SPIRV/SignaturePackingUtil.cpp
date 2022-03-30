//===--- SignaturePackingUtil.cpp - Util functions impl ----------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SignaturePackingUtil.h"

namespace clang {
namespace spirv {

namespace {

/// A class for managing stage input/output packed locations to avoid duplicate
/// uses of the same location and component.
class PackedLocationAndComponentSet {
public:
  PackedLocationAndComponentSet(SpirvBuilder &spirvBuilder,
                                llvm::function_ref<uint32_t(uint32_t)> nextLocs)
      : spvBuilder(spirvBuilder), assignLocs(nextLocs) {}

  bool assignLocAndComponent(const StageVar *var) {
    if (tryReuseLocations(var)) {
      return true;
    }
    return assignNewLocations(var);
  }

private:
  // Try to pack signature of |var| into unused component slots of already
  // assigned locations. Returns whether it succeeds or not.
  bool tryReuseLocations(const StageVar *var) {
    auto requiredLocsAndComponents = var->getLocationAndComponentCount();
    for (size_t startLoc = 0; startLoc < nextUnusedComponent.size();
         startLoc++) {
      bool canAssign = true;
      for (uint32_t i = 0; i < requiredLocsAndComponents.location; ++i) {
        if (startLoc + i >= nextUnusedComponent.size() ||
            nextUnusedComponent[startLoc + i] +
                    requiredLocsAndComponents.component >
                4) {
          canAssign = false;
          break;
        }
      }
      if (canAssign) {
        // Based on Vulkan spec "15.1.5. Component Assignment", a scalar or
        // two-component 64-bit data type must not specify a Component
        // decoration of 1 or 3.
        if (requiredLocsAndComponents.componentAlignment) {
          reuseLocations(var, startLoc, 2);
        } else {
          reuseLocations(var, startLoc, nextUnusedComponent[startLoc]);
        }
        return true;
      }
    }
    return false;
  }

  void reuseLocations(const StageVar *var, uint32_t startLoc,
                      uint32_t componentStart) {
    auto requiredLocsAndComponents = var->getLocationAndComponentCount();
    spvBuilder.decorateLocation(var->getSpirvInstr(), assignedLocs[startLoc]);
    spvBuilder.decorateComponent(var->getSpirvInstr(), componentStart);

    for (uint32_t i = 0; i < requiredLocsAndComponents.location; ++i) {
      nextUnusedComponent[startLoc + i] =
          componentStart + requiredLocsAndComponents.component;
    }
  }

  // Pack signature of |var| into new unified stage variables.
  bool assignNewLocations(const StageVar *var) {
    auto requiredLocsAndComponents = var->getLocationAndComponentCount();
    uint32_t loc = assignLocs(requiredLocsAndComponents.location);
    spvBuilder.decorateLocation(var->getSpirvInstr(), loc);

    uint32_t componentCount = requiredLocsAndComponents.component;
    for (uint32_t i = 0; i < requiredLocsAndComponents.location; ++i) {
      assignedLocs.push_back(loc + i);
      nextUnusedComponent.push_back(componentCount);
    }
    return true;
  }

private:
  SpirvBuilder &spvBuilder;
  llvm::function_ref<uint32_t(uint32_t)>
      assignLocs; ///< A function to assign a new location number.
  llvm::SmallVector<uint32_t, 8>
      assignedLocs; ///< A vector of assigned locations
  llvm::SmallVector<uint32_t, 8>
      nextUnusedComponent; ///< A vector to keep the starting unused component
                           ///< number in each assigned location
};

} // anonymous namespace

bool packSignatureInternal(
    const std::vector<const StageVar *> &vars,
    llvm::function_ref<bool(const StageVar *)> assignLocAndComponent,
    bool forInput, bool forPCF) {
  for (const auto *var : vars) {
    auto sigPointKind = var->getSigPoint()->GetKind();
    // HS has two types of outputs, one from the shader itself and another from
    // patch control function. They have HSCPOut and PCOut SigPointKind,
    // respectively. Since we do not know which one comes first at this moment,
    // we handle PCOut first. Likewise, DS has DSIn and DSCPIn as its inputs. We
    // handle DSIn first.
    if (forPCF) {
      if (sigPointKind != hlsl::SigPoint::Kind::PCOut &&
          sigPointKind != hlsl::SigPoint::Kind::DSIn) {
        continue;
      }
    } else {
      if (sigPointKind == hlsl::SigPoint::Kind::PCOut ||
          sigPointKind == hlsl::SigPoint::Kind::DSIn) {
        continue;
      }
    }

    if (!assignLocAndComponent(var)) {
      return false;
    }
  }
  return true;
}

bool packSignature(
    SpirvBuilder &spvBuilder,
    const std::vector<const StageVar *> &vars,
    llvm::function_ref<uint32_t(uint32_t)> nextLocs, bool forInput) {
  PackedLocationAndComponentSet packedLocSet(spvBuilder, nextLocs);
  auto assignLocationAndComponent = [&packedLocSet](const StageVar *var) {
    return packedLocSet.assignLocAndComponent(var);
  };
  return packSignatureInternal(vars, assignLocationAndComponent, forInput,
                               true) &&
         packSignatureInternal(vars, assignLocationAndComponent, forInput,
                               false);
}

} // end namespace spirv
} // end namespace clang
