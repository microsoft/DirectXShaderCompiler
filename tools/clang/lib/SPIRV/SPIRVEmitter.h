//===------- SPIRVEmitter.h - SPIR-V Binary Code Emitter --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file defines a SPIR-V emitter class that takes in HLSL AST and emits
//  SPIR-V binary words.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_SPIRVEMITTER_H
#define LLVM_CLANG_LIB_SPIRV_SPIRVEMITTER_H

#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "dxc/HLSL/DxilShaderModel.h"
#include "spirv/1.0/GLSL.std.450.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"

#include "DeclResultIdMapper.h"
#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// SPIR-V emitter class. It consumes the HLSL AST and emits SPIR-V words.
///
/// This class only overrides the HandleTranslationUnit() method; Traversing
/// through the AST is done manually instead of using ASTConsumer's harness.
class SPIRVEmitter : public ASTConsumer {
public:
  explicit SPIRVEmitter(CompilerInstance &ci);

  void HandleTranslationUnit(ASTContext &context) override;

  ASTContext &getASTContext() { return astContext; }
  ModuleBuilder &getModuleBuilder() { return theBuilder; }
  TypeTranslator &getTypeTranslator() { return typeTranslator; }
  DiagnosticsEngine &getDiagnosticsEngine() { return diags; }

  void doDecl(const Decl *decl);
  void doStmt(const Stmt *stmt, llvm::ArrayRef<const Attr *> attrs = {});
  uint32_t doExpr(const Expr *expr);

  /// Processes the given expression and emits SPIR-V instructions. If the
  /// result is a GLValue, does an additional load.
  ///
  /// This method is useful for cases where ImplicitCastExpr (LValueToRValue) is
  /// missing when using an lvalue as rvalue in the AST, e.g., DeclRefExpr will
  /// not be wrapped in ImplicitCastExpr (LValueToRValue) when appearing in
  /// HLSLVectorElementExpr since the generated HLSLVectorElementExpr itself can
  /// be lvalue or rvalue.
  uint32_t loadIfGLValue(const Expr *expr);

  /// Casts the given value from fromType to toType. fromType and toType should
  /// both be scalar or vector types of the same size.
  uint32_t castToType(uint32_t value, QualType fromType, QualType toType);

private:
  void doFunctionDecl(const FunctionDecl *decl);
  void doVarDecl(const VarDecl *decl);

  void doBreakStmt(const BreakStmt *stmt);
  void doDiscardStmt(const DiscardStmt *stmt);
  inline void doDeclStmt(const DeclStmt *stmt);
  void doForStmt(const ForStmt *, llvm::ArrayRef<const Attr *> attrs = {});
  void doIfStmt(const IfStmt *ifStmt);
  void doReturnStmt(const ReturnStmt *stmt);
  void doSwitchStmt(const SwitchStmt *stmt,
                    llvm::ArrayRef<const Attr *> attrs = {});
  void doWhileStmt(const WhileStmt *, llvm::ArrayRef<const Attr *> attrs = {});
  void doDoStmt(const DoStmt *, llvm::ArrayRef<const Attr *> attrs = {});
  void doContinueStmt(const ContinueStmt *);

  uint32_t doBinaryOperator(const BinaryOperator *expr);
  uint32_t doCallExpr(const CallExpr *callExpr);
  uint32_t doCastExpr(const CastExpr *expr);
  uint32_t doCompoundAssignOperator(const CompoundAssignOperator *expr);
  uint32_t doConditionalOperator(const ConditionalOperator *expr);
  uint32_t doCXXOperatorCallExpr(const CXXOperatorCallExpr *expr);
  uint32_t doExtMatrixElementExpr(const ExtMatrixElementExpr *expr);
  uint32_t doHLSLVectorElementExpr(const HLSLVectorElementExpr *expr);
  uint32_t doInitListExpr(const InitListExpr *expr);
  uint32_t doMemberExpr(const MemberExpr *expr);
  uint32_t doUnaryOperator(const UnaryOperator *expr);

private:
  /// Translates the return statement into its SPIR-V equivalent. Also generates
  /// necessary instructions for the entry function ensuring that the signature
  /// matches the SPIR-V requirements.
  void processReturnStmt(const ReturnStmt *stmt);

private:
  /// Translates the given frontend binary operator into its SPIR-V equivalent
  /// taking consideration of the operand type.
  spv::Op translateOp(BinaryOperator::Opcode op, QualType type);

  /// Generates the necessary instructions for assigning rhs to lhs. If lhsPtr
  /// is not zero, it will be used as the pointer from lhs instead of evaluating
  /// lhs again.
  uint32_t processAssignment(const Expr *lhs, const uint32_t rhs,
                             bool isCompoundAssignment, uint32_t lhsPtr = 0);

  /// Generates the necessary instructions for conducting the given binary
  /// operation on lhs and rhs. If lhsResultId is not nullptr, the evaluated
  /// pointer from lhs during the process will be written into it. If
  /// mandateGenOpcode is not spv::Op::Max, it will used as the SPIR-V opcode
  /// instead of deducing from Clang frontend opcode.
  uint32_t processBinaryOp(const Expr *lhs, const Expr *rhs,
                           const BinaryOperatorKind opcode,
                           const uint32_t resultType,
                           uint32_t *lhsResultId = nullptr,
                           const spv::Op mandateGenOpcode = spv::Op::Max);

  /// Generates SPIR-V instructions to initialize the given variable once.
  void initOnce(std::string varName, uint32_t varPtr, const Expr *varInit);

  /// Returns true if the given expression will be translated into a vector
  /// shuffle instruction in SPIR-V.
  ///
  /// We emit a vector shuffle instruction iff
  /// * We are not selecting only one element from the vector (OpAccessChain
  ///   or OpCompositeExtract for such case);
  /// * We are not selecting all elements in their original order (essentially
  ///   the original vector, no shuffling needed).
  bool isVectorShuffle(const Expr *expr);

  /// Returns true if the given CXXOperatorCallExpr is indexing into a vector or
  /// matrix using operator[].
  /// On success, writes the base vector/matrix into *base, and the indices into
  /// *index0 and *index1, if there are two levels of indexing. If there is only
  /// one level of indexing, writes the index into *index0 and nullptr into
  /// *index1.
  ///
  /// matrix [index0] [index1]         vector [index0]
  /// +-------------+
  ///  vector                     or
  /// +----------------------+         +-------------+
  ///         scalar                        scalar
  ///
  /// Assumes base, index0, and index1 are not nullptr.
  bool isVecMatIndexing(const CXXOperatorCallExpr *vecIndexExpr,
                        const Expr **base, const Expr **index0,
                        const Expr **index1);

  /// Condenses a sequence of HLSLVectorElementExpr starting from the given
  /// expr into one. Writes the original base into *basePtr and the condensed
  /// accessor into *flattenedAccessor.
  void condenseVectorElementExpr(
      const HLSLVectorElementExpr *expr, const Expr **basePtr,
      hlsl::VectorMemberAccessPositions *flattenedAccessor);

  /// Generates necessary SPIR-V instructions to create a vector splat out of
  /// the given scalarExpr. The generated vector will have the same element
  /// type as scalarExpr and of the given size. If resultIsConstant is not
  /// nullptr, writes true to it if the generated instruction is a constant.
  uint32_t createVectorSplat(const Expr *scalarExpr, uint32_t size,
                             bool *resultIsConstant = nullptr);

  /// Translates a floatN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatN * float.
  uint32_t tryToGenFloatVectorScale(const BinaryOperator *expr);

  /// Translates a floatMxN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatMxN * float.
  uint32_t tryToGenFloatMatrixScale(const BinaryOperator *expr);

  /// Tries to emit instructions for assigning to the given vector element
  /// accessing expression. Returns 0 if the trial fails and no instructions
  /// are generated.
  ///
  /// This method handles the cases that we are writing to neither one element
  /// or all elements in their original order. For other cases, 0 will be
  /// returned and the normal assignment process should be used.
  uint32_t tryToAssignToVectorElements(const Expr *lhs, const uint32_t rhs);

  /// Tries to emit instructions for assigning to the given matrix element
  /// accessing expression. Returns 0 if the trial fails and no instructions
  /// are generated.
  uint32_t tryToAssignToMatrixElements(const Expr *lhs, uint32_t rhs);

  /// Processes each vector within the given matrix by calling actOnEachVector.
  /// matrixVal should be the loaded value of the matrix. actOnEachVector takes
  /// three parameters for the current vector: the index, the <type-id>, and
  /// the value. It returns the <result-id> of the processed vector.
  uint32_t processEachVectorInMatrix(
      const Expr *matrix, const uint32_t matrixVal,
      llvm::function_ref<uint32_t(uint32_t, uint32_t, uint32_t)>
          actOnEachVector);

  /// Generates the necessary instructions for conducting the given binary
  /// operation on lhs and rhs.
  ///
  /// This method expects that both lhs and rhs are SPIR-V acceptable matrices.
  uint32_t processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
                                 const BinaryOperatorKind opcode);

  /// Collects all indices (SPIR-V constant values) from consecutive MemberExprs
  /// and writes into indices. Returns the real base (the first Expr that is not
  /// a MemberExpr).
  const Expr *collectStructIndices(const MemberExpr *expr,
                                   llvm::SmallVectorImpl<uint32_t> *indices);

private:
  /// Processes the given expr, casts the result into the given bool (vector)
  /// type and returns the <result-id> of the casted value.
  uint32_t castToBool(uint32_t value, QualType fromType, QualType toType);

  /// Processes the given expr, casts the result into the given integer (vector)
  /// type and returns the <result-id> of the casted value.
  uint32_t castToInt(uint32_t value, QualType fromType, QualType toType);

  /// Processes the given expr, casts the result into the given float (vector)
  /// type and returns the <result-id> of the casted value.
  uint32_t castToFloat(uint32_t value, QualType fromType, QualType toType);

private:
  /// Processes HLSL instrinsic functions.
  uint32_t processIntrinsicCallExpr(const CallExpr *);

  /// Processes the 'mul' intrinsic function.
  uint32_t processIntrinsicMul(const CallExpr *);

  /// Processes the 'dot' intrinsic function.
  uint32_t processIntrinsicDot(const CallExpr *);

  /// Processes the 'all' and 'any' intrinsic functions.
  uint32_t processIntrinsicAllOrAny(const CallExpr *, spv::Op);

  /// Processes the 'asfloat', 'asint', and 'asuint' intrinsic functions.
  uint32_t processIntrinsicAsType(const CallExpr *);

  /// Processes the 'sign' intrinsic function for float types.
  /// The FSign instruction in the GLSL instruction set returns a floating point
  /// result. The HLSL sign function, however, returns an integer. An extra
  /// casting from float to integer is therefore performed by this method.
  uint32_t processIntrinsicFloatSign(const CallExpr *);

  /// Processes the given intrinsic function call using the given GLSL
  /// extended instruction. If the given instruction cannot operate on matrices,
  /// it performs the instruction on each row of the matrix and uses composite
  /// construction to generate the resulting matrix.
  uint32_t processIntrinsicUsingGLSLInst(const CallExpr *, GLSLstd450 instr,
                                         bool canOperateOnMatrix);

private:
  /// Returns the <result-id> for constant value 0 of the given type.
  uint32_t getValueZero(QualType type);

  /// Returns the <result-id> for a constant zero vector of the given size and
  /// element type.
  uint32_t getVecValueZero(QualType elemType, uint32_t size);

  /// Returns the <result-id> for constant value 1 of the given type.
  uint32_t getValueOne(QualType type);

  /// Returns the <result-id> for a constant one vector of the given size and
  /// element type.
  uint32_t getVecValueOne(QualType elemType, uint32_t size);

  /// Returns the <result-id> for a constant one (vector) having the same
  /// element type as the given matrix type.
  ///
  /// If a 1x1 matrix is given, the returned value one will be a scalar;
  /// if a Mx1 or 1xN matrix is given, the returned value one will be a
  /// vector of size M or N; if a MxN matrix is given, the returned value
  /// one will be a vector of size N.
  uint32_t getMatElemValueOne(QualType type);

private:
  /// Translates the given frontend APValue into its SPIR-V equivalent for the
  /// given targetType.
  uint32_t translateAPValue(const APValue &value, const QualType targetType);

  /// Translates the given frontend APInt into its SPIR-V equivalent for the
  /// given targetType.
  uint32_t translateAPInt(const llvm::APInt &intValue, QualType targetType);

  /// Translates the given frontend APFloat into its SPIR-V equivalent for the
  /// given targetType.
  uint32_t translateAPFloat(const llvm::APFloat &floatValue,
                            QualType targetType);

  /// Translates the given HLSL loop attribute into SPIR-V loop control mask.
  /// Emits an error if the given attribute is not a loop attribute.
  spv::LoopControlMask translateLoopAttribute(const Attr &);

private:
  static spv::ExecutionModel
  getSpirvShaderStage(const hlsl::ShaderModel &model);

  void AddRequiredCapabilitiesForShaderModel();

  /// \brief Adds the execution mode for the given entry point based on the
  /// shader model.
  void AddExecutionModeForEntryPoint(uint32_t entryPointId);

private:
  /// \brief Returns true iff *all* the case values in the given switch
  /// statement are integer literals. In such cases OpSwitch can be used to
  /// represent the switch statement.
  /// We only care about the case values to be compared with the selector. They
  /// may appear in the top level CaseStmt or be nested in a CompoundStmt.Fall
  /// through cases will result in the second situation.
  bool allSwitchCasesAreIntegerLiterals(const Stmt *root);

  /// \brief Recursively discovers all CaseStmt and DefaultStmt under the
  /// sub-tree of the given root. Recursively goes down the tree iff it finds a
  /// CaseStmt, DefaultStmt, or CompoundStmt. It does not recurse on other
  /// statement types. For each discovered case, a basic block is created and
  /// registered within the module, and added as a successor to the current
  /// active basic block.
  ///
  /// Writes a vector of (integer, basic block label) pairs for all cases to the
  /// given 'targets' argument. If a DefaultStmt is found, it also returns the
  /// label for the default basic block through the defaultBB parameter. This
  /// method panics if it finds a case value that is not an integer literal.
  void discoverAllCaseStmtInSwitchStmt(
      const Stmt *root, uint32_t *defaultBB,
      std::vector<std::pair<uint32_t, uint32_t>> *targets);

  /// Flattens structured AST of the given switch statement into a vector of AST
  /// nodes and stores into flatSwitch.
  ///
  /// The AST for a switch statement may look arbitrarily different based on
  /// several factors such as placement of cases, placement of breaks, placement
  /// of braces, and fallthrough cases.
  ///
  /// A CaseStmt for instance is the child node of a CompoundStmt for
  /// regular cases and it is the child node of another CaseStmt for fallthrough
  /// cases.
  ///
  /// A BreakStmt for instance could be the child node of a CompoundStmt
  /// for regular cases, or the child node of a CaseStmt for some fallthrough
  /// cases.
  ///
  /// This method flattens the AST representation of a switch statement to make
  /// it easier to process for translation.
  /// For example:
  ///
  /// switch(a) {
  ///   case 1:
  ///     <Stmt1>
  ///   case 2:
  ///     <Stmt2>
  ///     break;
  ///   case 3:
  ///   case 4:
  ///     <Stmt4>
  ///     break;
  ///   deafult:
  ///     <Stmt5>
  /// }
  ///
  /// is flattened to the following vector:
  ///
  /// +-----+-----+-----+-----+-----+-----+-----+-----+-----+-------+-----+
  /// |Case1|Stmt1|Case2|Stmt2|Break|Case3|Case4|Stmt4|Break|Default|Stmt5|
  /// +-----+-----+-----+-----+-----+-----+-----+-----+-----+-------+-----+
  ///
  void flattenSwitchStmtAST(const Stmt *root,
                            std::vector<const Stmt *> *flatSwitch);

  void processCaseStmtOrDefaultStmt(const Stmt *stmt);

  void processSwitchStmtUsingSpirvOpSwitch(const SwitchStmt *switchStmt);
  /// Translates a switch statement into SPIR-V conditional branches.
  ///
  /// This is done by constructing AST if statements out of the cases using the
  /// following pattern:
  ///   if { ... } else if { ... } else if { ... } else { ... }
  /// And then calling the SPIR-V codegen methods for these if statements.
  ///
  /// Each case comparison is turned into an if statement, and the "then" body
  /// of the if statement will be the body of the case.
  /// If a default statements exists, it becomes the body of the "else"
  /// statement.
  void processSwitchStmtUsingIfStmts(const SwitchStmt *switchStmt);

private:
  /// \brief Returns the statement that the given break statement applies to.
  /// According to the spec, break statements can only apply to loops (do, for,
  /// while) or case statements inside a switch statement. The frontend ensures
  /// this is true (errors out otherwise).
  const Stmt *breakStmtScope(const BreakStmt *);

  /// \brief Returns true if the given BreakStmt is the last statement inside
  /// its case statement of the given switch statement. Panics if the given
  /// break statement is not inside the tree of the given switch statement.
  bool breakStmtIsLastStmtInCaseStmt(const BreakStmt *, const SwitchStmt *);

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(diagId);
  }

  /// \brief Wrapper method to create a warning message and report it
  /// in the diagnostic engine associated with this consumer
  template <unsigned N>
  DiagnosticBuilder emitWarning(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Warning, message);
    return diags.Report(diagId);
  }

private:
  CompilerInstance &theCompilerInstance;
  ASTContext &astContext;
  DiagnosticsEngine &diags;

  /// Entry function name and shader stage. Both of them are derived from the
  /// command line and should be const.
  const llvm::StringRef entryFunctionName;
  const hlsl::ShaderModel &shaderModel;

  SPIRVContext theContext;
  ModuleBuilder theBuilder;
  DeclResultIdMapper declIdMapper;
  TypeTranslator typeTranslator;

  /// A queue of decls reachable from the entry function. Decls inserted into
  /// this queue will persist to avoid duplicated translations. And we'd like
  /// a deterministic order of iterating the queue for finding the next decl
  /// to translate. So we need SetVector here.
  llvm::SetVector<const DeclaratorDecl *> workQueue;
  /// <result-id> for the entry function. Initially it is zero and will be reset
  /// when starting to translate the entry function.
  uint32_t entryFunctionId;
  /// The current function under traversal.
  const FunctionDecl *curFunction;

  /// Global variables that should be initialized once at the begining of the
  /// entry function.
  llvm::SmallVector<const VarDecl *, 4> toInitGloalVars;

  /// For loops, while loops, and switch statements may encounter "break"
  /// statements that alter their control flow. At any point the break statement
  /// is observed, the control flow jumps to the inner-most scope's merge block.
  /// For instance: the break in the following example should cause a branch to
  /// the SwitchMergeBB, not ForLoopMergeBB:
  /// for (...) {
  ///   switch(...) {
  ///     case 1: break;
  ///   }
  ///   <--- SwitchMergeBB
  /// }
  /// <---- ForLoopMergeBB
  /// This stack keeps track of the basic blocks to which branching could occur.
  std::stack<uint32_t> breakStack;

  /// Loops (do, while, for) may encounter "continue" statements that alter
  /// their control flow. At any point the continue statement is observed, the
  /// control flow jumps to the inner-most scope's continue block.
  /// This stack keeps track of the basic blocks to which such branching could
  /// occur.
  std::stack<uint32_t> continueStack;

  /// Maps a given statement to the basic block that is associated with it.
  llvm::DenseMap<const Stmt *, uint32_t> stmtBasicBlock;
};

void SPIRVEmitter::doDeclStmt(const DeclStmt *declStmt) {
  for (auto *decl : declStmt->decls())
    doDecl(decl);
}

} // end namespace spirv
} // end namespace clang

#endif
