//===------- SpirvEvalInfo.h - SPIR-V Evaluation Info -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file defines a struct containing SPIR-V information from evaluating
//  a Clang AST node.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_SPIRVEVALINFO_H
#define LLVM_CLANG_LIB_SPIRV_SPIRVEVALINFO_H

#include "spirv/1.0/spirv.hpp11"

namespace clang {
namespace spirv {

/// Memory layout rules
enum class LayoutRule {
  Void,
  GLSLStd140,
  GLSLStd430,
};

/// Struct contains SPIR-V information from evaluating a Clang AST node.
///
/// We need to report more information than just the <result-id> for SPIR-V:
/// * Constness: enables generating SPIR-V constant instructions
/// * Storage class: for getting correct (pointer) types
/// * Layout rule: for getting correct (pointer) types
/// * Relaxed precision: for emitting PrelaxedPrecision decoration
///
/// For most cases, the evaluation result of a Clang AST node will just be
/// a SPIR-V <result-id> that is not a constant, with storage class Function,
/// layout rule Void, and not relaxed precision.
///
/// The evaluation result of a Clang Expr can be considered as SPIR-V constant
/// as long as all its operands are SPIR-V constants and we are creating a
/// composite out of them.
///
/// We derive storage class or layout rule from a base variable. Access chains
/// and load/store/insertion/extraction of the base variable retains the same
/// storage class or layout rule. Other operations will likely turn the storage
/// class into Function and layout rule into Void.
///
/// Relaxed precision decorated on a variable propagates to all operations
/// involving that variable.
///
/// This struct has utilities to allow implicit constructing from or converting
/// to a uint32_t value (for <result-id>) since that's the most commonly used
/// information out of evaluating a Clang AST node. When doing such construction
/// or conversion, all other information are defaulted to what are meaningful
/// for a function temporary variable.
struct SpirvEvalInfo {
  /// Constructor from various evaluation information
  SpirvEvalInfo(uint32_t id)
      : resultId(id), isConst(false), storageClass(spv::StorageClass::Function),
        layoutRule(LayoutRule::Void), isRelaxedPrecision(false) {}
  SpirvEvalInfo(uint32_t id, spv::StorageClass sc, LayoutRule rule)
      : resultId(id), isConst(false), storageClass(sc), layoutRule(rule),
        isRelaxedPrecision(false) {}

  static SpirvEvalInfo withConst(uint32_t id) {
    SpirvEvalInfo info = id;
    info.isConst = true;
    return info;
  }

  static SpirvEvalInfo withRelaxedPrecision(uint32_t id) {
    SpirvEvalInfo info = id;
    info.isRelaxedPrecision = true;
    return info;
  }

  /// Returns a new SpirvEvalInfo with an updated SPIR-V <result-id>. Keeps the
  /// other fields the same.
  SpirvEvalInfo substResultId(uint32_t newId) const {
    SpirvEvalInfo info = *this;
    info.resultId = newId;
    return info;
  }

  /// Handy implicit conversion to get the <result-id>.
  operator uint32_t() const { return resultId; }

  /// Handly implicit conversion to test whether the <result-id> is valid.
  operator bool() const { return resultId != 0; }

  uint32_t resultId; ///< SPIR-V <result-id>
  bool isConst;      ///< Whether the <result-id> is for a SPIR-V constant
  spv::StorageClass storageClass;
  LayoutRule layoutRule;
  bool isRelaxedPrecision;
};

} // end namespace spirv
} // end namespace clang

#endif
