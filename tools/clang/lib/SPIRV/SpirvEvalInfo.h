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

#include "spirv/unified1/spirv.hpp11"

namespace clang {
namespace spirv {

/// Struct contains SPIR-V information from evaluating a Clang AST node.
///
/// We need to report more information than just the <result-id> for SPIR-V:
///
/// * Storage class: for getting correct (pointer) types
/// * Layout rule: for getting correct (pointer) types
/// * LValue or RValue
/// * Constness: enables generating SPIR-V constant instructions
/// * Relaxed precision: for emitting PrelaxedPrecision decoration
///
/// For most cases, the evaluation result of a Clang AST node will just be
/// a SPIR-V <result-id> with storage class Function, layout rule Void,
/// not constant, and not relaxed precision.
///
/// So the constructor only takes in a <result-id>. It initializes storage
/// class to Function, layout rule to Void, constness to false, relaxed
/// precision to false. It sets the <result-id> as lvalue. Setters are
/// provided to tweak these bits.
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
class SpirvEvalInfo {
public:
  /// Implicit constructor
  inline SpirvEvalInfo(uint32_t id);

  /// Changes the <result-id> in this eval info.
  inline SpirvEvalInfo &setResultId(uint32_t id);
  /// Returns a new eval info with the given <result-id> and inheriting all
  /// other bits from this eval info.
  inline SpirvEvalInfo substResultId(uint32_t id) const;

  /// Handy implicit conversion to get the <result-id>.
  operator uint32_t() const { return resultId; }

  /// Handly implicit conversion to test whether the <result-id> is valid.
  operator bool() const { return resultId != 0; }

  inline SpirvEvalInfo &setContainsAliasComponent(bool);
  bool containsAliasComponent() const { return containsAlias; }

  inline SpirvEvalInfo &setStorageClass(spv::StorageClass sc);
  spv::StorageClass getStorageClass() const { return storageClass; }

  inline SpirvEvalInfo &setLayoutRule(SpirvLayoutRule rule);
  SpirvLayoutRule getLayoutRule() const { return layoutRule; }

  inline SpirvEvalInfo &setRValue(bool rvalue = true);
  bool isRValue() const { return isRValue_; }

  inline SpirvEvalInfo &setConstant();
  bool isConstant() const { return isConstant_; }

  inline SpirvEvalInfo &setSpecConstant();
  bool isSpecConstant() const { return isSpecConstant_; }

  inline SpirvEvalInfo &setRelaxedPrecision();
  bool isRelaxedPrecision() const { return isRelaxedPrecision_; }

  inline SpirvEvalInfo &setNonUniform(bool nu = true);
  bool isNonUniform() const { return isNonUniform_; }

private:
  uint32_t resultId;
  /// Indicates whether this evaluation result contains alias variables
  ///
  /// This field should only be true for stand-alone alias variables, which is
  /// of pointer-to-pointer type, or struct variables containing alias fields.
  /// After dereferencing the alias variable, this should be set to false to let
  /// CodeGen fall back to normal handling path.
  ///
  /// Note: legalization specific code
  bool containsAlias;

  spv::StorageClass storageClass;
  SpirvLayoutRule layoutRule;

  bool isRValue_;
  bool isConstant_;
  bool isSpecConstant_;
  bool isRelaxedPrecision_;
  bool isNonUniform_;
};

SpirvEvalInfo::SpirvEvalInfo(uint32_t id)
    : resultId(id), containsAlias(false),
      storageClass(spv::StorageClass::Function),
      layoutRule(SpirvLayoutRule::Void), isRValue_(false), isConstant_(false),
      isSpecConstant_(false), isRelaxedPrecision_(false), isNonUniform_(false) {
}

SpirvEvalInfo &SpirvEvalInfo::setResultId(uint32_t id) {
  resultId = id;
  return *this;
}

SpirvEvalInfo SpirvEvalInfo::substResultId(uint32_t newId) const {
  SpirvEvalInfo info = *this;
  info.resultId = newId;
  return info;
}

SpirvEvalInfo &SpirvEvalInfo::setContainsAliasComponent(bool contains) {
  containsAlias = contains;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setStorageClass(spv::StorageClass sc) {
  storageClass = sc;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setLayoutRule(SpirvLayoutRule rule) {
  layoutRule = rule;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setRValue(bool rvalue) {
  isRValue_ = rvalue;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setConstant() {
  isConstant_ = true;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setSpecConstant() {
  // Specialization constant is also a kind of constant.
  isConstant_ = isSpecConstant_ = true;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setRelaxedPrecision() {
  isRelaxedPrecision_ = true;
  return *this;
}

SpirvEvalInfo &SpirvEvalInfo::setNonUniform(bool nu) {
  isNonUniform_ = nu;
  return *this;
}

} // end namespace spirv
} // end namespace clang

#endif
