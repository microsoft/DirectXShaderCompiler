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
#include "dxc/HlslIntrinsicOp.h"
#include "spirv/1.0/GLSL.std.450.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/EmitSPIRVOptions.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"

#include "DeclResultIdMapper.h"
#include "SpirvEvalInfo.h"
#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// SPIR-V emitter class. It consumes the HLSL AST and emits SPIR-V words.
///
/// This class only overrides the HandleTranslationUnit() method; Traversing
/// through the AST is done manually instead of using ASTConsumer's harness.
class SPIRVEmitter : public ASTConsumer {
public:
  SPIRVEmitter(CompilerInstance &ci, const EmitSPIRVOptions &options);

  void HandleTranslationUnit(ASTContext &context) override;

  ASTContext &getASTContext() { return astContext; }
  ModuleBuilder &getModuleBuilder() { return theBuilder; }
  TypeTranslator &getTypeTranslator() { return typeTranslator; }
  DiagnosticsEngine &getDiagnosticsEngine() { return diags; }

  void doDecl(const Decl *decl);
  void doStmt(const Stmt *stmt, llvm::ArrayRef<const Attr *> attrs = {});
  SpirvEvalInfo doExpr(const Expr *expr);

  /// Processes the given expression and emits SPIR-V instructions. If the
  /// result is a GLValue, does an additional load.
  ///
  /// This method is useful for cases where ImplicitCastExpr (LValueToRValue) is
  /// missing when using an lvalue as rvalue in the AST, e.g., DeclRefExpr will
  /// not be wrapped in ImplicitCastExpr (LValueToRValue) when appearing in
  /// HLSLVectorElementExpr since the generated HLSLVectorElementExpr itself can
  /// be lvalue or rvalue.
  SpirvEvalInfo loadIfGLValue(const Expr *expr);

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

  SpirvEvalInfo doArraySubscriptExpr(const ArraySubscriptExpr *expr);
  SpirvEvalInfo doBinaryOperator(const BinaryOperator *expr);
  SpirvEvalInfo doCallExpr(const CallExpr *callExpr);
  SpirvEvalInfo doCastExpr(const CastExpr *expr);
  SpirvEvalInfo doCompoundAssignOperator(const CompoundAssignOperator *expr);
  uint32_t doConditionalOperator(const ConditionalOperator *expr);
  SpirvEvalInfo doCXXMemberCallExpr(const CXXMemberCallExpr *expr);
  SpirvEvalInfo doCXXOperatorCallExpr(const CXXOperatorCallExpr *expr);
  SpirvEvalInfo doExtMatrixElementExpr(const ExtMatrixElementExpr *expr);
  SpirvEvalInfo doHLSLVectorElementExpr(const HLSLVectorElementExpr *expr);
  SpirvEvalInfo doInitListExpr(const InitListExpr *expr);
  SpirvEvalInfo doMemberExpr(const MemberExpr *expr);
  SpirvEvalInfo doUnaryOperator(const UnaryOperator *expr);

private:
  /// Translates the given frontend binary operator into its SPIR-V equivalent
  /// taking consideration of the operand type.
  spv::Op translateOp(BinaryOperator::Opcode op, QualType type);

  /// Generates SPIR-V instructions for the given normal (non-intrinsic and
  /// non-operator) standalone or member function call.
  uint32_t processCall(const CallExpr *expr);

  /// Generates the necessary instructions for assigning rhs to lhs. If lhsPtr
  /// is not zero, it will be used as the pointer from lhs instead of evaluating
  /// lhs again.
  SpirvEvalInfo processAssignment(const Expr *lhs, const SpirvEvalInfo &rhs,
                                  bool isCompoundAssignment,
                                  SpirvEvalInfo lhsPtr = 0);

  /// Generates SPIR-V instructions to store rhsVal into lhsPtr. This will be
  /// recursive if valType is a composite type.
  void storeValue(const SpirvEvalInfo &lhsPtr, const SpirvEvalInfo &rhsVal,
                  QualType valType);

  /// Generates the necessary instructions for conducting the given binary
  /// operation on lhs and rhs. If lhsResultId is not nullptr, the evaluated
  /// pointer from lhs during the process will be written into it. If
  /// mandateGenOpcode is not spv::Op::Max, it will used as the SPIR-V opcode
  /// instead of deducing from Clang frontend opcode.
  SpirvEvalInfo processBinaryOp(const Expr *lhs, const Expr *rhs,
                                BinaryOperatorKind opcode, uint32_t resultType,
                                SpirvEvalInfo *lhsInfo = nullptr,
                                spv::Op mandateGenOpcode = spv::Op::Max);

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

  /// \brief Returns true if the given CXXOperatorCallExpr is indexing into a
  /// Buffer/RWBuffer/Texture/RWTexture using operator[].
  /// On success, writes the base buffer into *base if base is not nullptr, and
  /// writes the index into *index if index is not nullptr.
  bool isBufferTextureIndexing(const CXXOperatorCallExpr *,
                               const Expr **base = nullptr,
                               const Expr **index = nullptr);

  /// \brief Returns true if the given CXXOperatorCallExpr is the .mips[][]
  /// access into a Texture or .sample[][] access into Texture2DMS(Array). On
  /// success, writes base texture object into *base if base is not nullptr,
  /// writes the index into *index if index is not nullptr, and writes the mip
  /// level (lod) to *lod if lod is not nullptr.
  bool isTextureMipsSampleIndexing(const CXXOperatorCallExpr *indexExpr,
                                   const Expr **base = nullptr,
                                   const Expr **index = nullptr,
                                   const Expr **lod = nullptr);

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
  SpirvEvalInfo createVectorSplat(const Expr *scalarExpr, uint32_t size);

  /// Splits the given vector into the last element and the rest (as a new
  /// vector).
  void splitVecLastElement(QualType vecType, uint32_t vec, uint32_t *residual,
                           uint32_t *lastElement);

  /// Translates a floatN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatN * float.
  SpirvEvalInfo tryToGenFloatVectorScale(const BinaryOperator *expr);

  /// Translates a floatMxN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatMxN * float.
  SpirvEvalInfo tryToGenFloatMatrixScale(const BinaryOperator *expr);

  /// Tries to emit instructions for assigning to the given vector element
  /// accessing expression. Returns 0 if the trial fails and no instructions
  /// are generated.
  ///
  /// This method handles the cases that we are writing to neither one element
  /// or all elements in their original order. For other cases, 0 will be
  /// returned and the normal assignment process should be used.
  SpirvEvalInfo tryToAssignToVectorElements(const Expr *lhs,
                                            const SpirvEvalInfo &rhs);

  /// Tries to emit instructions for assigning to the given matrix element
  /// accessing expression. Returns 0 if the trial fails and no instructions
  /// are generated.
  SpirvEvalInfo tryToAssignToMatrixElements(const Expr *lhs,
                                            const SpirvEvalInfo &rhs);

  /// Tries to emit instructions for assigning to the given RWBuffer/RWTexture
  /// object. Returns 0 if the trial fails and no instructions are generated.
  SpirvEvalInfo tryToAssignToRWBufferRWTexture(const Expr *lhs,
                                               const SpirvEvalInfo &rhs);

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
  SpirvEvalInfo processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
                                      const BinaryOperatorKind opcode);

  /// Collects all indices (SPIR-V constant values) from consecutive MemberExprs
  /// or ArraySubscriptExprs or operator[] calls and writes into indices.
  /// Returns the real base.
  const Expr *
  collectArrayStructIndices(const Expr *expr,
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

  /// Processes the 'clip' intrinsic function. Discards the current pixel if the
  /// specified value is less than zero.
  uint32_t processIntrinsicClip(const CallExpr *);

  /// Processes the 'clamp' intrinsic function.
  uint32_t processIntrinsicClamp(const CallExpr *);

  /// Processes the 'frexp' intrinsic function.
  uint32_t processIntrinsicFrexp(const CallExpr *);

  /// Processes the 'lit' intrinsic function.
  uint32_t processIntrinsicLit(const CallExpr *);

  /// Processes the 'modf' intrinsic function.
  uint32_t processIntrinsicModf(const CallExpr *);

  /// Processes the 'mul' intrinsic function.
  uint32_t processIntrinsicMul(const CallExpr *);

  /// Processes the 'dot' intrinsic function.
  uint32_t processIntrinsicDot(const CallExpr *);

  /// Processes the 'log10' intrinsic function.
  uint32_t processIntrinsicLog10(const CallExpr *);

  /// Processes the 'all' and 'any' intrinsic functions.
  uint32_t processIntrinsicAllOrAny(const CallExpr *, spv::Op);

  /// Processes the 'asfloat', 'asint', and 'asuint' intrinsic functions.
  uint32_t processIntrinsicAsType(const CallExpr *);

  /// Processes the 'saturate' intrinsic function.
  uint32_t processIntrinsicSaturate(const CallExpr *);

  /// Processes the 'sincos' intrinsic function.
  uint32_t processIntrinsicSinCos(const CallExpr *);

  /// Processes the 'isFinite' intrinsic function.
  uint32_t processIntrinsicIsFinite(const CallExpr *);

  /// Processes the 'rcp' intrinsic function.
  uint32_t processIntrinsicRcp(const CallExpr *);

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

  /// Processes the given intrinsic function call using the given SPIR-V
  /// instruction. If the given instruction cannot operate on matrices, it
  /// performs the instruction on each row of the matrix and uses composite
  /// construction to generate the resulting matrix.
  uint32_t processIntrinsicUsingSpirvInst(const CallExpr *, spv::Op,
                                          bool canOperateOnMatrix);

  /// Processes the given intrinsic member call.
  SpirvEvalInfo processIntrinsicMemberCall(const CXXMemberCallExpr *expr,
                                           hlsl::IntrinsicOp opcode);

  /// Processes Interlocked* intrinsic functions.
  uint32_t processIntrinsicInterlockedMethod(const CallExpr *,
                                             hlsl::IntrinsicOp);

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
  /// \brief Performs a FlatConversion implicit cast. Fills an instance of the
  /// given type with initializer <result-id>. The initializer is of type
  /// initType.
  uint32_t processFlatConversion(const QualType type, const QualType initType,
                                 uint32_t initId);

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

  /// Tries to evaluate the given Expr as a constant and returns the <result-id>
  /// if success. Otherwise, returns 0.
  uint32_t tryToEvaluateAsConst(const Expr *expr);

private:
  /// Translates the given HLSL loop attribute into SPIR-V loop control mask.
  /// Emits an error if the given attribute is not a loop attribute.
  spv::LoopControlMask translateLoopAttribute(const Attr &);

  static spv::ExecutionModel
  getSpirvShaderStage(const hlsl::ShaderModel &model);

  void AddRequiredCapabilitiesForShaderModel();

  /// \brief Adds the execution mode for the given entry point based on the
  /// shader model.
  void AddExecutionModeForEntryPoint(uint32_t entryPointId);

  /// \brief Adds necessary execution modes for the hull/domain shaders based on
  /// the HLSL attributes of the entry point function.
  /// In the case of hull shaders, also writes the number of output control
  /// points to *numOutputControlPoints. Returns true on success, and false on
  /// failure.
  bool processTessellationShaderAttributes(const FunctionDecl *entryFunction,
                                           uint32_t *numOutputControlPoints);

  /// \brief Adds necessary execution modes for the geometry shader based on the
  /// HLSL attributes of the entry point function.
  bool processGeometryShaderAttributes(const FunctionDecl *entryFunction);

  /// \brief Emits a wrapper function for the entry function and returns true
  /// on success.
  ///
  /// The wrapper function loads the values of all stage input variables and
  /// creates composites as expected by the source code entry function. It then
  /// calls the source code entry point and writes out stage output variables
  /// by extracting sub-values from the return value. In this way, we can handle
  /// the source code entry point as a normal function.
  ///
  /// The wrapper function is also responsible for initializing global static
  /// variables for some cases.
  bool emitEntryFunctionWrapper(const FunctionDecl *entryFunction,
                                uint32_t entryFuncId);

  /// \brief Performs the following operations for the Hull shader:
  /// * Creates an output variable which is an Array containing results for all
  /// control points.
  ///
  /// * If the Patch Constant Function (PCF) takes the Hull main entry function
  /// results (OutputPatch), it creates a temporary function-scope variable that
  /// is then passed to the PCF.
  ///
  /// * Adds a control barrier (OpControlBarrier) to ensure all invocations are
  /// done before PCF is called.
  ///
  /// * Prepares the necessary parameters to pass to the PCF (Can be one or more
  /// of InputPatch, OutputPatch, PrimitiveId).
  ///
  /// * The execution thread with ControlPointId (invocationID) of 0 calls the
  /// PCF. e.g. if(id == 0) pcf();
  ///
  /// * Gathers the results of the PCF and assigns them to stage output
  /// variables.
  ///
  /// The method panics if it is called for any shader kind other than Hull
  /// shaders.
  bool processHullEntryPointOutputAndPatchConstFunc(
      const FunctionDecl *hullMainFuncDecl, uint32_t retType, uint32_t retVal,
      uint32_t numOutputControlPoints, uint32_t outputControlPointId,
      uint32_t primitiveId, uint32_t hullMainInputPatch);

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
  /// Handles the optional offset argument in the given method call at the given
  /// argument index.
  /// If there exists an offset argument, writes the <result-id> to either
  /// *constOffset or *varOffset, depending on the constantness of the offset.
  void handleOptionalOffsetInMethodCall(const CXXMemberCallExpr *expr,
                                        uint32_t index, uint32_t *constOffset,
                                        uint32_t *varOffset);

  /// \brief Processes .Load() method call for Buffer/RWBuffer and texture
  /// objects.
  SpirvEvalInfo processBufferTextureLoad(const CXXMemberCallExpr *);

  /// \brief Loads one element from the given Buffer/RWBuffer/Texture object at
  /// the given location. The type of the loaded element matches the type in the
  /// declaration for the Buffer/Texture object.
  uint32_t processBufferTextureLoad(const Expr *object, uint32_t location,
                                    uint32_t constOffset = 0,
                                    uint32_t varOffst = 0, uint32_t lod = 0);

  /// \brief Processes .Sample() and .Gather() method calls for texture objects.
  uint32_t processTextureSampleGather(const CXXMemberCallExpr *expr,
                                      bool isSample);

  /// \brief Processes .SampleBias() and .SampleLevel() method calls for texture
  /// objects.
  uint32_t processTextureSampleBiasLevel(const CXXMemberCallExpr *expr,
                                         bool isBias);

  /// \brief Processes .SampleGrad() method call for texture objects.
  uint32_t processTextureSampleGrad(const CXXMemberCallExpr *expr);

  /// \brief Processes .SampleCmp() or .SampleCmpLevelZero() method call for
  /// texture objects.
  uint32_t processTextureSampleCmpCmpLevelZero(const CXXMemberCallExpr *expr,
                                               bool isCmp);

  /// \brief Handles .Gather{|Cmp}{Red|Green|Blue|Alpha}() calls on texture
  /// types.
  uint32_t processTextureGatherRGBACmpRGBA(const CXXMemberCallExpr *expr,
                                           bool isCmp, uint32_t component);

  /// \brief Handles .GatherCmp() calls on texture types.
  uint32_t processTextureGatherCmp(const CXXMemberCallExpr *expr);

  /// \brief Returns the calculated level-of-detail (a single float value) for
  /// the given texture. Handles intrinsic HLSL CalculateLevelOfDetail function.
  uint32_t processTextureLevelOfDetail(const CXXMemberCallExpr *expr);

  /// \brief Processes the .GetDimensions() call on supported objects.
  uint32_t processGetDimensions(const CXXMemberCallExpr *);

  /// \brief Queries the given (RW)Buffer/(RW)Texture image in the given expr
  /// for the requested information. Based on the dimension of the image, the
  /// following info can be queried: width, height, depth, number of mipmap
  /// levels.
  uint32_t processBufferTextureGetDimensions(const CXXMemberCallExpr *);

  /// \brief Generates an OpAccessChain instruction for the given
  /// (RW)StructuredBuffer.Load() method call.
  SpirvEvalInfo processStructuredBufferLoad(const CXXMemberCallExpr *expr);

  /// \brief Increments or decrements the counter for RW/Append/Consume
  /// structured buffer.
  uint32_t incDecRWACSBufferCounter(const CXXMemberCallExpr *, bool isInc);

  /// \brief Loads numWords 32-bit unsigned integers or stores numWords 32-bit
  /// unsigned integers (based on the doStore parameter) to the given
  /// ByteAddressBuffer. Loading is allowed from a ByteAddressBuffer or
  /// RWByteAddressBuffer. Storing is allowed only to RWByteAddressBuffer.
  /// Panics if it is not the case.
  uint32_t processByteAddressBufferLoadStore(const CXXMemberCallExpr *,
                                             uint32_t numWords, bool doStore);

  /// \brief Processes the GetDimensions intrinsic function call on a
  /// (RW)ByteAddressBuffer by querying the image in the given expr.
  uint32_t processByteAddressBufferStructuredBufferGetDimensions(
      const CXXMemberCallExpr *);

  /// \brief Processes the Interlocked* intrinsic function call on a
  /// RWByteAddressBuffer.
  uint32_t processRWByteAddressBufferAtomicMethods(hlsl::IntrinsicOp opcode,
                                                   const CXXMemberCallExpr *);

  /// \brief Generates SPIR-V instructions for the .Append()/.Consume() call on
  /// the given {Append|Consume}StructuredBuffer. Returns the <result-id> of
  /// the loaded value for .Consume; returns zero for .Append().
  SpirvEvalInfo processACSBufferAppendConsume(const CXXMemberCallExpr *expr);

private:
  /// \brief Wrapper method to create a fatal error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitFatalError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Fatal, message);
    return diags.Report(diagId);
  }

  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(loc, diagId);
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

  EmitSPIRVOptions spirvOptions;

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
  /// The SPIR-V function parameter for the current this object.
  uint32_t curThis;

  /// Whether the translated SPIR-V binary needs legalization.
  ///
  /// The following cases will require legalization:
  /// * Opaque types (textures, samplers) within structs
  ///
  /// If this is true, SPIRV-Tools legalization passes will be executed after
  /// the translation to legalize the generated SPIR-V binary.
  bool needsLegalization;

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

  /// This is the Patch Constant Function. This function is not explicitly
  /// called from the entry point function.
  FunctionDecl *patchConstFunc;
};

void SPIRVEmitter::doDeclStmt(const DeclStmt *declStmt) {
  for (auto *decl : declStmt->decls())
    doDecl(decl);
}

} // end namespace spirv
} // end namespace clang

#endif
