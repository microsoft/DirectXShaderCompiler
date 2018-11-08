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

#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/HlslIntrinsicOp.h"
#include "spirv/unified1/GLSL.std.450.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/FeatureManager.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvContext.h"
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
  SPIRVEmitter(CompilerInstance &ci);

  void HandleTranslationUnit(ASTContext &context) override;

  ASTContext &getASTContext() { return astContext; }
  SpirvBuilder &getModuleBuilder() { return spvBuilder; }
  TypeTranslator &getTypeTranslator() { return typeTranslator; }
  DiagnosticsEngine &getDiagnosticsEngine() { return diags; }

  void doDecl(const Decl *decl);
  void doStmt(const Stmt *stmt, llvm::ArrayRef<const Attr *> attrs = {});
  SpirvInstruction *doExpr(const Expr *expr);

  /// Processes the given expression and emits SPIR-V instructions. If the
  /// result is a GLValue, does an additional load.
  ///
  /// This method is useful for cases where ImplicitCastExpr (LValueToRValue) is
  /// missing when using an lvalue as rvalue in the AST, e.g., DeclRefExpr will
  /// not be wrapped in ImplicitCastExpr (LValueToRValue) when appearing in
  /// HLSLVectorElementExpr since the generated HLSLVectorElementExpr itself can
  /// be lvalue or rvalue.
  SpirvInstruction *loadIfGLValue(const Expr *expr);

  /// Casts the given value from fromType to toType. fromType and toType should
  /// both be scalar or vector types of the same size.
  SpirvInstruction *castToType(SpirvInstruction *value, QualType fromType,
                               QualType toType, SourceLocation);

private:
  void doFunctionDecl(const FunctionDecl *decl);
  void doVarDecl(const VarDecl *decl);
  void doRecordDecl(const RecordDecl *decl);
  void doHLSLBufferDecl(const HLSLBufferDecl *decl);

  void doBreakStmt(const BreakStmt *stmt);
  void doDiscardStmt(const DiscardStmt *stmt);
  inline void doDeclStmt(const DeclStmt *stmt);
  void doForStmt(const ForStmt *, llvm::ArrayRef<const Attr *> attrs = {});
  void doIfStmt(const IfStmt *ifStmt, llvm::ArrayRef<const Attr *> attrs = {});
  void doReturnStmt(const ReturnStmt *stmt);
  void doSwitchStmt(const SwitchStmt *stmt,
                    llvm::ArrayRef<const Attr *> attrs = {});
  void doWhileStmt(const WhileStmt *, llvm::ArrayRef<const Attr *> attrs = {});
  void doDoStmt(const DoStmt *, llvm::ArrayRef<const Attr *> attrs = {});
  void doContinueStmt(const ContinueStmt *);

  SpirvInstruction *doArraySubscriptExpr(const ArraySubscriptExpr *expr);
  SpirvInstruction *doBinaryOperator(const BinaryOperator *expr);
  SpirvInstruction *doCallExpr(const CallExpr *callExpr);
  SpirvInstruction *doCastExpr(const CastExpr *expr);
  SpirvInstruction *doCompoundAssignOperator(const CompoundAssignOperator *);
  SpirvInstruction *doConditionalOperator(const ConditionalOperator *expr);
  SpirvInstruction *doCXXMemberCallExpr(const CXXMemberCallExpr *expr);
  SpirvInstruction *doCXXOperatorCallExpr(const CXXOperatorCallExpr *expr);
  SpirvInstruction *doExtMatrixElementExpr(const ExtMatrixElementExpr *expr);
  SpirvInstruction *doHLSLVectorElementExpr(const HLSLVectorElementExpr *expr);
  SpirvInstruction *doInitListExpr(const InitListExpr *expr);
  SpirvInstruction *doMemberExpr(const MemberExpr *expr);
  SpirvInstruction *doUnaryOperator(const UnaryOperator *expr);

  /// Overload with pre computed SpirvEvalInfo.
  ///
  /// The given expr will not be evaluated again.
  SpirvInstruction *loadIfGLValue(const Expr *expr, SpirvInstruction *info);

  /// Loads the pointer of the aliased-to-variable if the given expression is a
  /// DeclRefExpr referencing an alias variable. See DeclResultIdMapper for
  /// more explanation regarding this.
  ///
  /// Note: legalization specific code
  SpirvInstruction *loadIfAliasVarRef(const Expr *expr);

  /// Loads the pointer of the aliased-to-variable and ajusts aliasVarInfo
  /// accordingly if aliasVarExpr is referencing an alias variable. Returns true
  /// if aliasVarInfo is changed, false otherwise.
  ///
  /// Note: legalization specific code
  bool loadIfAliasVarRef(const Expr *aliasVarExpr,
                         SpirvInstruction **aliasVarInstr);

private:
  /// Translates the given frontend binary operator into its SPIR-V equivalent
  /// taking consideration of the operand type.
  spv::Op translateOp(BinaryOperator::Opcode op, QualType type);

  spv::Op translateWaveOp(hlsl::IntrinsicOp op, QualType type, SourceLocation);

  /// Generates SPIR-V instructions for the given normal (non-intrinsic and
  /// non-operator) standalone or member function call.
  SpirvInstruction *processCall(const CallExpr *expr);

  /// Generates the necessary instructions for assigning rhs to lhs. If lhsPtr
  /// is not zero, it will be used as the pointer from lhs instead of evaluating
  /// lhs again.
  SpirvInstruction *processAssignment(const Expr *lhs, SpirvInstruction *rhs,
                                      bool isCompoundAssignment,
                                      SpirvInstruction *lhsPtr = nullptr);

  /// Generates SPIR-V instructions to store rhsVal into lhsPtr. This will be
  /// recursive if lhsValType is a composite type. rhsExpr will be used as a
  /// reference to adjust the CodeGen if not nullptr.
  void storeValue(SpirvInstruction *lhsPtr, SpirvInstruction *rhsVal,
                  QualType lhsValType);

  /// Decomposes and reconstructs the given srcVal of the given valType to meet
  /// the requirements of the dstLR layout rule.
  SpirvInstruction *reconstructValue(SpirvInstruction *srcVal, QualType valType,
                                     SpirvLayoutRule dstLR);

  /// Generates the necessary instructions for conducting the given binary
  /// operation on lhs and rhs.
  ///
  /// computationType is the type for LHS and RHS when doing computation, while
  /// resultType is the type of the whole binary operation. They can be
  /// different for compound assignments like <some-int-value> *=
  /// <some-float-value>, where computationType is float and resultType is int.
  ///
  /// If lhsResultId is not nullptr, the evaluated pointer from lhs during the
  /// process will be written into it. If mandateGenOpcode is not spv::Op::Max,
  /// it will used as the SPIR-V opcode instead of deducing from Clang frontend
  /// opcode.
  SpirvInstruction *processBinaryOp(const Expr *lhs, const Expr *rhs,
                                    BinaryOperatorKind opcode,
                                    QualType computationType,
                                    QualType resultType, SourceRange,
                                    SpirvInstruction **lhsInfo = nullptr,
                                    spv::Op mandateGenOpcode = spv::Op::Max);

  /// Generates SPIR-V instructions to initialize the given variable once.
  void initOnce(QualType varType, std::string varName, SpirvVariable *,
                const Expr *varInit);

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
  SpirvInstruction *createVectorSplat(const Expr *scalarExpr, uint32_t size);

  /// Splits the given vector into the last element and the rest (as a new
  /// vector).
  void splitVecLastElement(QualType vecType, SpirvInstruction *vec,
                           SpirvInstruction **residual,
                           SpirvInstruction **lastElement);

  /// Converts a vector value into the given struct type with its element type's
  /// <result-id> as elemTypeId.
  ///
  /// Assumes the vector and the struct have matching number of elements. Panics
  /// otherwise.
  SpirvInstruction *convertVectorToStruct(QualType structType,
                                          QualType elemType,
                                          SpirvInstruction *vector);

  /// Translates a floatN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatN * float.
  SpirvInstruction *tryToGenFloatVectorScale(const BinaryOperator *expr);

  /// Translates a floatMxN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatMxN * float.
  SpirvInstruction *tryToGenFloatMatrixScale(const BinaryOperator *expr);

  /// Tries to emit instructions for assigning to the given vector element
  /// accessing expression. Returns 0 if the trial fails and no instructions
  /// are generated.
  SpirvInstruction *tryToAssignToVectorElements(const Expr *lhs,
                                                SpirvInstruction *rhs);

  /// Tries to emit instructions for assigning to the given matrix element
  /// accessing expression. Returns 0 if the trial fails and no instructions
  /// are generated.
  SpirvInstruction *tryToAssignToMatrixElements(const Expr *lhs,
                                                SpirvInstruction *rhs);

  /// Tries to emit instructions for assigning to the given RWBuffer/RWTexture
  /// object. Returns 0 if the trial fails and no instructions are generated.
  SpirvInstruction *tryToAssignToRWBufferRWTexture(const Expr *lhs,
                                                   SpirvInstruction *rhs);

  /// Processes each vector within the given matrix by calling actOnEachVector.
  /// matrixVal should be the loaded value of the matrix. actOnEachVector takes
  /// three parameters for the current vector: the index, the <type-id>, and
  /// the value. It returns the <result-id> of the processed vector.
  SpirvInstruction *processEachVectorInMatrix(
      const Expr *matrix, SpirvInstruction *matrixVal,
      llvm::function_ref<SpirvInstruction *(uint32_t, QualType,
                                            SpirvInstruction *)>
          actOnEachVector);

  /// Translates the given varDecl into a spec constant.
  void createSpecConstant(const VarDecl *varDecl);

  /// Generates the necessary instructions for conducting the given binary
  /// operation on lhs and rhs.
  ///
  /// This method expects that both lhs and rhs are SPIR-V acceptable matrices.
  SpirvInstruction *processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
                                          const BinaryOperatorKind opcode,
                                          SourceRange);

  /// Creates a temporary local variable in the current function of the given
  /// varType and varName. Initializes the variable with the given initValue.
  /// Returns the instruction pointer for the variable.
  SpirvVariable *createTemporaryVar(QualType varType, llvm::StringRef varName,
                                    SpirvInstruction *initValue);

  /// Collects all indices from consecutive MemberExprs
  /// or ArraySubscriptExprs or operator[] calls. If indices is not null,
  /// SPIR-V constant values are written into *indices. Returns the real base.
  /// If rawIndices is not null, the raw integer indices will be written to
  /// *rawIndices, and the base returned can be nullptr, which means some
  /// indices are not constant.
  /// Either indices or rawIndices must be non-null.
  const Expr *
  collectArrayStructIndices(const Expr *expr,
                            llvm::SmallVectorImpl<SpirvInstruction *> *indices,
                            llvm::SmallVectorImpl<uint32_t> *rawIndices);

  /// Creates an access chain to index into the given SPIR-V evaluation result
  /// and returns the new SPIR-V evaluation result.
  SpirvInstruction *
  turnIntoElementPtr(QualType baseType, SpirvInstruction *base,
                     QualType elemType,
                     const llvm::SmallVector<SpirvInstruction *, 4> &indices);

private:
  /// Validates that vk::* attributes are used correctly and returns false if
  /// errors are found.
  bool validateVKAttributes(const NamedDecl *decl);

private:
  /// Converts the given value from the bitwidth of 'fromType' to the bitwidth
  /// of 'toType'. If the two have the same bitwidth, returns the value itself.
  /// If resultType is not nullptr, the resulting value's type will be written
  /// to resultType. Panics if the given types are not scalar or vector of
  /// float/integer type.
  uint32_t convertBitwidth(uint32_t value, QualType fromType, QualType toType,
                           uint32_t *resultType = nullptr);

  /// Processes the given expr, casts the result into the given bool (vector)
  /// type and returns the <result-id> of the casted value.
  SpirvInstruction *castToBool(SpirvInstruction *value, QualType fromType,
                               QualType toType);

  /// Processes the given expr, casts the result into the given integer (vector)
  /// type and returns the <result-id> of the casted value.
  SpirvInstruction *castToInt(SpirvInstruction *value, QualType fromType,
                              QualType toType, SourceLocation);

  /// Processes the given expr, casts the result into the given float (vector)
  /// type and returns the <result-id> of the casted value.
  SpirvInstruction *castToFloat(SpirvInstruction *value, QualType fromType,
                                QualType toType, SourceLocation);

private:
  /// Processes HLSL instrinsic functions.
  SpirvInstruction *processIntrinsicCallExpr(const CallExpr *);

  /// Processes the 'clip' intrinsic function. Discards the current pixel if the
  /// specified value is less than zero.
  SpirvInstruction *processIntrinsicClip(const CallExpr *);

  /// Processes the 'dst' intrinsic function.
  SpirvInstruction *processIntrinsicDst(const CallExpr *);

  /// Processes the 'clamp' intrinsic function.
  SpirvInstruction *processIntrinsicClamp(const CallExpr *);

  /// Processes the 'frexp' intrinsic function.
  SpirvInstruction *processIntrinsicFrexp(const CallExpr *);

  /// Processes the 'ldexp' intrinsic function.
  SpirvInstruction *processIntrinsicLdexp(const CallExpr *);

  /// Processes the 'D3DCOLORtoUBYTE4' intrinsic function.
  SpirvInstruction *processD3DCOLORtoUBYTE4(const CallExpr *);

  /// Processes the 'lit' intrinsic function.
  SpirvInstruction *processIntrinsicLit(const CallExpr *);

  /// Processes the 'GroupMemoryBarrier', 'GroupMemoryBarrierWithGroupSync',
  /// 'DeviceMemoryBarrier', 'DeviceMemoryBarrierWithGroupSync',
  /// 'AllMemoryBarrier', and 'AllMemoryBarrierWithGroupSync' intrinsic
  /// functions.
  SpirvInstruction *processIntrinsicMemoryBarrier(const CallExpr *,
                                                  bool isDevice, bool groupSync,
                                                  bool isAllBarrier);

  /// Processes the 'mad' intrinsic function.
  uint32_t processIntrinsicMad(const CallExpr *);

  /// Processes the 'modf' intrinsic function.
  SpirvInstruction *processIntrinsicModf(const CallExpr *);

  /// Processes the 'msad4' intrinsic function.
  SpirvInstruction *processIntrinsicMsad4(const CallExpr *);

  /// Processes the 'mul' intrinsic function.
  SpirvInstruction *processIntrinsicMul(const CallExpr *);

  /// Transposes a non-floating point matrix and returns the result-id of the
  /// transpose.
  SpirvInstruction *processNonFpMatrixTranspose(QualType matType,
                                                SpirvInstruction *matrix);

  /// Processes the dot product of two non-floating point vectors. The SPIR-V
  /// OpDot only accepts float vectors. Assumes that the two vectors are of the
  /// same size and have the same element type (elemType).
  SpirvInstruction *processNonFpDot(SpirvInstruction *vec1Id,
                                    SpirvInstruction *vec2Id, uint32_t vecSize,
                                    QualType elemType);

  /// Processes the multiplication of a *non-floating point* matrix by a scalar.
  /// Assumes that the matrix element type and the scalar type are the same.
  SpirvInstruction *processNonFpScalarTimesMatrix(QualType scalarType,
                                                  SpirvInstruction *scalar,
                                                  QualType matType,
                                                  SpirvInstruction *matrix);

  /// Processes the multiplication of a *non-floating point* matrix by a vector.
  /// Assumes the matrix element type and the vector element type are the same.
  /// Notice that the vector in this case is a "row vector" and will be
  /// multiplied by the matrix columns (dot product). As a result, the given
  /// matrix must be transposed in order to easily get each column. If
  /// 'matrixTranspose' is non-zero, it will be used as the transpose matrix
  /// result-id; otherwise the function will perform the transpose itself.
  SpirvInstruction *
  processNonFpVectorTimesMatrix(QualType vecType, SpirvInstruction *vector,
                                QualType matType, SpirvInstruction *matrix,
                                SpirvInstruction *matrixTranspose = nullptr);

  /// Processes the multiplication of a vector by a *non-floating point* matrix.
  /// Assumes the matrix element type and the vector element type are the same.
  SpirvInstruction *processNonFpMatrixTimesVector(QualType matType,
                                                  SpirvInstruction *matrix,
                                                  QualType vecType,
                                                  SpirvInstruction *vector);

  /// Processes a non-floating point matrix multiplication. Assumes that the
  /// number of columns in lhs matrix is the same as number of rows in the rhs
  /// matrix. Also assumes that the two matrices have the same element type.
  SpirvInstruction *processNonFpMatrixTimesMatrix(QualType lhsType,
                                                  SpirvInstruction *lhs,
                                                  QualType rhsType,
                                                  SpirvInstruction *rhs);

  /// Processes the 'dot' intrinsic function.
  SpirvInstruction *processIntrinsicDot(const CallExpr *);

  /// Processes the 'log10' intrinsic function.
  SpirvInstruction *processIntrinsicLog10(const CallExpr *);

  /// Processes the 'all' and 'any' intrinsic functions.
  SpirvInstruction *processIntrinsicAllOrAny(const CallExpr *, spv::Op);

  /// Processes the 'asfloat', 'asint', and 'asuint' intrinsic functions.
  SpirvInstruction *processIntrinsicAsType(const CallExpr *);

  /// Processes the 'saturate' intrinsic function.
  SpirvInstruction *processIntrinsicSaturate(const CallExpr *);

  /// Processes the 'sincos' intrinsic function.
  SpirvInstruction *processIntrinsicSinCos(const CallExpr *);

  /// Processes the 'isFinite' intrinsic function.
  SpirvInstruction *processIntrinsicIsFinite(const CallExpr *);

  /// Processes the 'rcp' intrinsic function.
  SpirvInstruction *processIntrinsicRcp(const CallExpr *);

  /// Processes the 'sign' intrinsic function for float types.
  /// The FSign instruction in the GLSL instruction set returns a floating point
  /// result. The HLSL sign function, however, returns an integer. An extra
  /// casting from float to integer is therefore performed by this method.
  SpirvInstruction *processIntrinsicFloatSign(const CallExpr *);

  /// Processes the 'f16to32' intrinsic function.
  SpirvInstruction *processIntrinsicF16ToF32(const CallExpr *);
  /// Processes the 'f32tof16' intrinsic function.
  SpirvInstruction *processIntrinsicF32ToF16(const CallExpr *);

  /// Processes the given intrinsic function call using the given GLSL
  /// extended instruction. If the given instruction cannot operate on matrices,
  /// it performs the instruction on each row of the matrix and uses composite
  /// construction to generate the resulting matrix.
  SpirvInstruction *processIntrinsicUsingGLSLInst(const CallExpr *,
                                                  GLSLstd450 instr,
                                                  bool canOperateOnMatrix);

  /// Processes the given intrinsic function call using the given SPIR-V
  /// instruction. If the given instruction cannot operate on matrices, it
  /// performs the instruction on each row of the matrix and uses composite
  /// construction to generate the resulting matrix.
  SpirvInstruction *processIntrinsicUsingSpirvInst(const CallExpr *, spv::Op,
                                                   bool canOperateOnMatrix);

  /// Processes the given intrinsic member call.
  SpirvInstruction *processIntrinsicMemberCall(const CXXMemberCallExpr *expr,
                                               hlsl::IntrinsicOp opcode);

  /// Processes Interlocked* intrinsic functions.
  SpirvInstruction *processIntrinsicInterlockedMethod(const CallExpr *,
                                                      hlsl::IntrinsicOp);
  /// Processes SM6.0 wave query intrinsic calls.
  SpirvInstruction *processWaveQuery(const CallExpr *, spv::Op opcode);

  /// Processes SM6.0 wave vote intrinsic calls.
  SpirvInstruction *processWaveVote(const CallExpr *, spv::Op opcode);

  /// Processes SM6.0 wave active/prefix count bits.
  SpirvInstruction *processWaveCountBits(const CallExpr *,
                                         spv::GroupOperation groupOp);

  /// Processes SM6.0 wave reduction or scan/prefix intrinsic calls.
  SpirvInstruction *processWaveReductionOrPrefix(const CallExpr *, spv::Op op,
                                                 spv::GroupOperation groupOp);

  /// Processes SM6.0 wave broadcast intrinsic calls.
  SpirvInstruction *processWaveBroadcast(const CallExpr *);

  /// Processes SM6.0 quad-wide shuffle.
  SpirvInstruction *processWaveQuadWideShuffle(const CallExpr *,
                                               hlsl::IntrinsicOp op);

  /// Processes the NonUniformResourceIndex intrinsic function.
  SpirvInstruction *processIntrinsicNonUniformResourceIndex(const CallExpr *);

private:
  /// Returns the <result-id> for constant value 0 of the given type.
  SpirvConstant *getValueZero(QualType type);

  /// Returns the <result-id> for a constant zero vector of the given size and
  /// element type.
  SpirvConstant *getVecValueZero(QualType elemType, uint32_t size);

  /// Returns the <result-id> for constant value 1 of the given type.
  SpirvConstant *getValueOne(QualType type);

  /// Returns the <result-id> for a constant one vector of the given size and
  /// element type.
  SpirvConstant *getVecValueOne(QualType elemType, uint32_t size);

  /// Returns the <result-id> for a constant one (vector) having the same
  /// element type as the given matrix type.
  ///
  /// If a 1x1 matrix is given, the returned value one will be a scalar;
  /// if a Mx1 or 1xN matrix is given, the returned value one will be a
  /// vector of size M or N; if a MxN matrix is given, the returned value
  /// one will be a vector of size N.
  SpirvConstant *getMatElemValueOne(QualType type);

  /// Returns a SPIR-V constant equal to the bitwdith of the given type minus
  /// one. The returned constant has the same component count and bitwidth as
  /// the given type.
  SpirvConstant *getMaskForBitwidthValue(QualType type);

private:
  /// \brief Performs a FlatConversion implicit cast. Fills an instance of the
  /// given type with initializer <result-id>. The initializer is of type
  /// initType.
  SpirvInstruction *processFlatConversion(const QualType type,
                                          const QualType initType,
                                          SpirvInstruction *initId,
                                          SourceLocation);

private:
  /// Translates the given frontend APValue into its SPIR-V equivalent for the
  /// given targetType.
  SpirvConstant *translateAPValue(const APValue &value,
                                  const QualType targetType);

  /// Translates the given frontend APInt into its SPIR-V equivalent for the
  /// given targetType.
  SpirvConstant *translateAPInt(const llvm::APInt &intValue,
                                QualType targetType);

  /// Translates the given frontend APFloat into its SPIR-V equivalent for the
  /// given targetType.
  SpirvConstant *translateAPFloat(llvm::APFloat floatValue,
                                  QualType targetType);

  /// Tries to evaluate the given Expr as a constant and returns the <result-id>
  /// if success. Otherwise, returns 0.
  SpirvConstant *tryToEvaluateAsConst(const Expr *expr);

  /// Tries to evaluate the given APFloat as a 32-bit float. If the evaluation
  /// can be performed without loss, it returns the <result-id> of the SPIR-V
  /// constant for that value. Returns zero otherwise.
  SpirvConstant *tryToEvaluateAsFloat32(const llvm::APFloat &);

  /// Tries to evaluate the given APInt as a 32-bit integer. If the evaluation
  /// can be performed without loss, it returns the <result-id> of the SPIR-V
  /// constant for that value.
  SpirvConstant *tryToEvaluateAsInt32(const llvm::APInt &, bool isSigned);

  /// Returns true iff the given expression is a literal integer that cannot be
  /// represented in a 32-bit integer type or a literal float that cannot be
  /// represented in a 32-bit float type without losing info. Returns false
  /// otherwise.
  bool isLiteralLargerThan32Bits(const Expr *expr);

private:
  /// Translates the given HLSL loop attribute into SPIR-V loop control mask.
  /// Emits an error if the given attribute is not a loop attribute.
  spv::LoopControlMask translateLoopAttribute(const Stmt *, const Attr &);

  static spv::ExecutionModel
  getSpirvShaderStage(const hlsl::ShaderModel &model);

  void AddRequiredCapabilitiesForShaderModel();

  /// \brief Adds necessary execution modes for the hull/domain shaders based on
  /// the HLSL attributes of the entry point function.
  /// In the case of hull shaders, also writes the number of output control
  /// points to *numOutputControlPoints. Returns true on success, and false on
  /// failure.
  bool processTessellationShaderAttributes(const FunctionDecl *entryFunction,
                                           uint32_t *numOutputControlPoints);

  /// \brief Adds necessary execution modes for the geometry shader based on the
  /// HLSL attributes of the entry point function. Also writes the array size of
  /// the input, which depends on the primitive type, to *arraySize.
  bool processGeometryShaderAttributes(const FunctionDecl *entryFunction,
                                       uint32_t *arraySize);

  /// \brief Adds necessary execution modes for the pixel shader based on the
  /// HLSL attributes of the entry point function.
  void processPixelShaderAttributes(const FunctionDecl *decl);

  /// \brief Adds necessary execution modes for the compute shader based on the
  /// HLSL attributes of the entry point function.
  void processComputeShaderAttributes(const FunctionDecl *entryFunction);

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
  bool processHSEntryPointOutputAndPCF(const FunctionDecl *hullMainFuncDecl,
                                       uint32_t retType, uint32_t retVal,
                                       uint32_t numOutputControlPoints,
                                       uint32_t outputControlPointId,
                                       uint32_t primitiveId, uint32_t viewId,
                                       uint32_t hullMainInputPatch);

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
      const Stmt *root, SpirvBasicBlock **defaultBB,
      std::vector<std::pair<uint32_t, SpirvBasicBlock *>> *targets);

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
  /// Handles the offset argument in the given method call at the given argument
  /// index. Panics if the argument at the given index does not exist. Writes
  /// the <result-id> to either *constOffset or *varOffset, depending on the
  /// constantness of the offset.
  void handleOffsetInMethodCall(const CXXMemberCallExpr *expr, uint32_t index,
                                SpirvInstruction **constOffset,
                                SpirvInstruction **varOffset);

  /// \brief Processes .Load() method call for Buffer/RWBuffer and texture
  /// objects.
  SpirvInstruction *processBufferTextureLoad(const CXXMemberCallExpr *);

  /// \brief Loads one element from the given Buffer/RWBuffer/Texture object at
  /// the given location. The type of the loaded element matches the type in the
  /// declaration for the Buffer/Texture object.
  /// If residencyCodeId is not zero,  the SPIR-V instruction for storing the
  /// resulting residency code will also be emitted.
  SpirvInstruction *processBufferTextureLoad(const Expr *object,
                                             SpirvInstruction *location,
                                             SpirvInstruction *constOffset,
                                             SpirvInstruction *varOffset,
                                             SpirvInstruction *lod,
                                             SpirvInstruction *residencyCode);

  /// \brief Processes .Sample() and .Gather() method calls for texture objects.
  SpirvInstruction *processTextureSampleGather(const CXXMemberCallExpr *expr,
                                               bool isSample);

  /// \brief Processes .SampleBias() and .SampleLevel() method calls for texture
  /// objects.
  SpirvInstruction *processTextureSampleBiasLevel(const CXXMemberCallExpr *expr,
                                                  bool isBias);

  /// \brief Processes .SampleGrad() method call for texture objects.
  SpirvInstruction *processTextureSampleGrad(const CXXMemberCallExpr *expr);

  /// \brief Processes .SampleCmp() or .SampleCmpLevelZero() method call for
  /// texture objects.
  SpirvInstruction *
  processTextureSampleCmpCmpLevelZero(const CXXMemberCallExpr *expr,
                                      bool isCmp);

  /// \brief Handles .Gather{|Cmp}{Red|Green|Blue|Alpha}() calls on texture
  /// types.
  SpirvInstruction *
  processTextureGatherRGBACmpRGBA(const CXXMemberCallExpr *expr, bool isCmp,
                                  uint32_t component);

  /// \brief Handles .GatherCmp() calls on texture types.
  SpirvInstruction *processTextureGatherCmp(const CXXMemberCallExpr *expr);

  /// \brief Returns the calculated level-of-detail (a single float value) for
  /// the given texture. Handles intrinsic HLSL CalculateLevelOfDetail or
  /// CalculateLevelOfDetailUnclamped function depending on the given unclamped
  /// parameter.
  SpirvInstruction *processTextureLevelOfDetail(const CXXMemberCallExpr *expr,
                                                bool unclamped);

  /// \brief Processes the .GetDimensions() call on supported objects.
  SpirvInstruction *processGetDimensions(const CXXMemberCallExpr *);

  /// \brief Queries the given (RW)Buffer/(RW)Texture image in the given expr
  /// for the requested information. Based on the dimension of the image, the
  /// following info can be queried: width, height, depth, number of mipmap
  /// levels.
  SpirvInstruction *
  processBufferTextureGetDimensions(const CXXMemberCallExpr *);

  /// \brief Generates an OpAccessChain instruction for the given
  /// (RW)StructuredBuffer.Load() method call.
  SpirvInstruction *processStructuredBufferLoad(const CXXMemberCallExpr *expr);

  /// \brief Increments or decrements the counter for RW/Append/Consume
  /// structured buffer. If loadObject is true, the object upon which the call
  /// is made will be evaluated and translated into SPIR-V.
  SpirvInstruction *incDecRWACSBufferCounter(const CXXMemberCallExpr *call,
                                             bool isInc,
                                             bool loadObject = true);

  /// Assigns the counter variable associated with srcExpr to the one associated
  /// with dstDecl if the dstDecl is an internal RW/Append/Consume structured
  /// buffer. Returns false if there is no associated counter variable for
  /// srcExpr or dstDecl.
  ///
  /// Note: legalization specific code
  bool tryToAssignCounterVar(const DeclaratorDecl *dstDecl,
                             const Expr *srcExpr);
  bool tryToAssignCounterVar(const Expr *dstExpr, const Expr *srcExpr);

  /// Returns the counter variable's information associated with the entity
  /// represented by the given decl.
  ///
  /// This method only handles final alias structured buffers, which means
  /// AssocCounter#1 and AssocCounter#2.
  const CounterIdAliasPair *getFinalACSBufferCounter(const Expr *expr);
  /// This method handles AssocCounter#3 and AssocCounter#4.
  const CounterVarFields *
  getIntermediateACSBufferCounter(const Expr *expr,
                                  llvm::SmallVector<uint32_t, 4> *indices);

  /// Gets or creates an ImplicitParamDecl to represent the implicit object
  /// parameter of the given method.
  const ImplicitParamDecl *
  getOrCreateDeclForMethodObject(const CXXMethodDecl *method);

  /// \brief Loads numWords 32-bit unsigned integers or stores numWords 32-bit
  /// unsigned integers (based on the doStore parameter) to the given
  /// ByteAddressBuffer. Loading is allowed from a ByteAddressBuffer or
  /// RWByteAddressBuffer. Storing is allowed only to RWByteAddressBuffer.
  /// Panics if it is not the case.
  SpirvInstruction *processByteAddressBufferLoadStore(const CXXMemberCallExpr *,
                                                      uint32_t numWords,
                                                      bool doStore);

  /// \brief Processes the GetDimensions intrinsic function call on a
  /// (RW)ByteAddressBuffer by querying the image in the given expr.
  SpirvInstruction *processByteAddressBufferStructuredBufferGetDimensions(
      const CXXMemberCallExpr *);

  /// \brief Processes the Interlocked* intrinsic function call on a
  /// RWByteAddressBuffer.
  SpirvInstruction *
  processRWByteAddressBufferAtomicMethods(hlsl::IntrinsicOp opcode,
                                          const CXXMemberCallExpr *);

  /// \brief Processes the GetSamplePosition intrinsic method call on a
  /// Texture2DMS(Array).
  SpirvInstruction *processGetSamplePosition(const CXXMemberCallExpr *);

  /// \brief Processes the SubpassLoad intrinsic function call on a
  /// SubpassInput(MS).
  SpirvInstruction *processSubpassLoad(const CXXMemberCallExpr *);

  /// \brief Generates SPIR-V instructions for the .Append()/.Consume() call on
  /// the given {Append|Consume}StructuredBuffer. Returns the <result-id> of
  /// the loaded value for .Consume; returns zero for .Append().
  SpirvInstruction *
  processACSBufferAppendConsume(const CXXMemberCallExpr *expr);

  /// \brief Generates SPIR-V instructions to emit the current vertex in GS.
  SpirvInstruction *processStreamOutputAppend(const CXXMemberCallExpr *expr);

  /// \brief Generates SPIR-V instructions to end emitting the current
  /// primitive in GS.
  SpirvInstruction *processStreamOutputRestart(const CXXMemberCallExpr *expr);

  /// \brief Emulates GetSamplePosition() for standard sample settings, i.e.,
  /// with 1, 2, 4, 8, or 16 samples. Returns float2(0) for other cases.
  SpirvInstruction *emitGetSamplePosition(SpirvInstruction *sampleCount,
                                          SpirvInstruction *sampleIndex);

private:
  /// \brief Takes a vector of size 4, and returns a vector of size 1 or 2 or 3
  /// or 4. Creates a CompositeExtract or VectorShuffle instruction to extract
  /// a scalar or smaller vector from the beginning of the input vector if
  /// necessary. Assumes that 'fromId' is the <result-id> of a vector of size 4.
  /// Panics if the target vector size is not 1, 2, 3, or 4.
  SpirvInstruction *extractVecFromVec4(SpirvInstruction *fromInstr,
                                       uint32_t targetVecSize,
                                       QualType targetElemType);

  /// \brief Creates SPIR-V instructions for sampling the given image.
  /// It utilizes the ModuleBuilder's createImageSample and it ensures that the
  /// returned type is handled correctly.
  /// HLSL image sampling methods may return a scalar, vec1, vec2, vec3, or
  /// vec4. But non-Dref image sampling instructions in SPIR-V must always
  /// return a vec4. As a result, an extra processing step is necessary.
  SpirvInstruction *createImageSample(
      QualType retType, QualType imageType, SpirvInstruction *image,
      SpirvInstruction *sampler, bool isNonUniform,
      SpirvInstruction *coordinate, SpirvInstruction *compareVal,
      SpirvInstruction *bias, SpirvInstruction *lod,
      std::pair<SpirvInstruction *, SpirvInstruction *> grad,
      SpirvInstruction *constOffset, SpirvInstruction *varOffset,
      SpirvInstruction *constOffsets, SpirvInstruction *sample,
      SpirvInstruction *minLod, SpirvInstruction *residencyCodeId);

  /// \brief Emit an OpLine instruction for the given source location.
  void emitDebugLine(SourceLocation);

private:
  /// \brief Wrapper method to create a fatal error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitFatalError(const char (&message)[N],
                                   SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Fatal, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create a warning message and report it
  /// in the diagnostic engine associated with this consumer
  template <unsigned N>
  DiagnosticBuilder emitWarning(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Warning, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create a note message and report it
  /// in the diagnostic engine associated with this consumer
  template <unsigned N>
  DiagnosticBuilder emitNote(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Note, message);
    return diags.Report(loc, diagId);
  }

private:
  CompilerInstance &theCompilerInstance;
  ASTContext &astContext;
  DiagnosticsEngine &diags;

  SpirvCodeGenOptions &spirvOptions;

  /// Entry function name and shader stage. Both of them are derived from the
  /// command line and should be const.
  const llvm::StringRef entryFunctionName;
  const hlsl::ShaderModel &shaderModel;

  SPIRVContext theContext;
  SpirvContext spvContext;
  FeatureManager featureManager;
  ModuleBuilder theBuilder;
  SpirvBuilder spvBuilder;
  TypeTranslator typeTranslator;
  DeclResultIdMapper declIdMapper;

  /// A queue of decls reachable from the entry function. Decls inserted into
  /// this queue will persist to avoid duplicated translations. And we'd like
  /// a deterministic order of iterating the queue for finding the next decl
  /// to translate. So we need SetVector here.
  llvm::SetVector<const DeclaratorDecl *> workQueue;

  /// <result-id> for the entry function. Initially it is zero and will be reset
  /// when starting to translate the entry function.
  SpirvFunction *entryFunction;
  /// The current function under traversal.
  const FunctionDecl *curFunction;
  /// The SPIR-V function parameter for the current this object.
  SpirvInstruction *curThis;

  /// The source location of a push constant block we have previously seen.
  /// Invalid means no push constant blocks defined thus far.
  SourceLocation seenPushConstantAt;

  /// Indicates whether the current emitter is in specialization constant mode:
  /// all 32-bit scalar constants will be translated into OpSpecConstant.
  bool isSpecConstantMode;

  /// Indicates that we have found a NonUniformResourceIndex call when
  /// traversing.
  /// This field is used to convery information in a bottom-up manner; if we
  /// have something like `aResource[NonUniformResourceIndex(aIndex)]`, we need
  /// to attach `aResource` with proper decorations.
  bool foundNonUniformResourceIndex;

  /// Whether the translated SPIR-V binary needs legalization.
  ///
  /// The following cases will require legalization:
  ///
  /// 1. Opaque types (textures, samplers) within structs
  /// 2. Structured buffer aliasing
  /// 3. Using SPIR-V instructions not allowed in the currect shader stage
  ///
  /// This covers the first and third case.
  ///
  /// If this is true, SPIRV-Tools legalization passes will be executed after
  /// the translation to legalize the generated SPIR-V binary.
  ///
  /// Note: legalization specific code
  bool needsLegalization;

  /// Mapping from methods to the decls to represent their implicit object
  /// parameters
  ///
  /// We need this map because that we need to update the associated counters on
  /// the implicit object when invoking method calls. The ImplicitParamDecl
  /// mapped to serves as a key to find the associated counters in
  /// DeclResultIdMapper.
  ///
  /// Note: legalization specific code
  llvm::DenseMap<const CXXMethodDecl *, const ImplicitParamDecl *> thisDecls;

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
  std::stack<SpirvBasicBlock *> breakStack;

  /// Loops (do, while, for) may encounter "continue" statements that alter
  /// their control flow. At any point the continue statement is observed, the
  /// control flow jumps to the inner-most scope's continue block.
  /// This stack keeps track of the basic blocks to which such branching could
  /// occur.
  std::stack<SpirvBasicBlock *> continueStack;

  /// Maps a given statement to the basic block that is associated with it.
  llvm::DenseMap<const Stmt *, SpirvBasicBlock *> stmtBasicBlock;

  /// This is the Patch Constant Function. This function is not explicitly
  /// called from the entry point function.
  FunctionDecl *patchConstFunc;

  /// The <result-id> of the OpString containing the main source file's path.
  uint32_t mainSourceFileId;
};

void SPIRVEmitter::doDeclStmt(const DeclStmt *declStmt) {
  for (auto *decl : declStmt->decls())
    doDecl(decl);
}

} // end namespace spirv
} // end namespace clang

#endif
