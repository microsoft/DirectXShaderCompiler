//===--- GlPerVertex.h - For handling gl_PerVertex members -------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_GLPERVERTEX_H
#define LLVM_CLANG_LIB_SPIRV_GLPERVERTEX_H

#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"

#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// The class for representing special gl_PerVertex builtin interface block.
/// The Position, PointSize, ClipDistance, and CullDistance builtin should
/// be handled by this class, except for
/// * Position builtin used in GS output and PS input,
/// * PointSize builtin used in GS output.
///
/// Although the Vulkan spec does not require this directly, it seems the only
/// way to avoid violating the spec is to group the Position, ClipDistance, and
/// CullDistance builtins together into a struct. That's also how GLSL handles
/// these builtins. In GLSL, this struct is called gl_PerVertex.
///
/// This struct should appear as the entry point parameters but it should not
/// have location assignment. We can have two such blocks at most: one for
/// input, one for output.
///
/// Reading/writing of the ClipDistance/CullDistance builtin is not as
/// straightforward as other builtins. This is because in HLSL, we can have
/// multiple entities annotated with SV_ClipDistance/SV_CullDistance and they
/// can be float or vector of float type. For example,
///
///   float2 var1 : SV_ClipDistance2,
///   float  var2 : SV_ClipDistance0,
///   float3 var3 : SV_ClipDistance1,
///
/// But in Vulkan, ClipDistance/CullDistance is required to be a float array.
/// So we need to combine these variables into one single float array. The way
/// we do it is by sorting all entities according to the SV_ClipDistance/
/// SV_CullDistance index, and concatenating them tightly. So for the above,
/// var2 will take the first two floats in the array, var3 will take the next
/// three, and var1 will take the next two. In total, we have an size-6 float
/// array for ClipDistance builtin.
class GlPerVertex {
public:
  GlPerVertex(const hlsl::ShaderModel &sm, ASTContext &context,
              ModuleBuilder &builder, TypeTranslator &translator);

  /// Records a declaration of SV_ClipDistance/SV_CullDistance so later
  /// we can caculate the ClipDistance/CullDistance array layout.
  bool recordClipCullDistanceDecl(const DeclaratorDecl *decl, bool asInput);

  /// Calculates the layout for ClipDistance/CullDistance arrays.
  void calculateClipCullDistanceArraySize();

  /// Emits SPIR-V code for the input and/or ouput gl_PerVertex builtin
  /// interface blocks. If inputArrayLength is not zero, the input gl_PerVertex
  /// will have an additional arrayness of the given size. Similarly for
  /// outputArrayLength.
  ///
  /// Note that this method should be called after recordClipCullDistanceDecl()
  /// and calculateClipCullDistanceArraySize().
  void generateVars(uint32_t inputArrayLength, uint32_t outputArrayLength);

  /// Returns the <result-id>s for stage input variables.
  llvm::SmallVector<uint32_t, 4> getStageInVars() const;
  /// Returns the <result-id>s for stage output variables.
  llvm::SmallVector<uint32_t, 4> getStageOutVars() const;

  /// Requires the ClipDistance/CullDistance capability if we've seen
  /// definition of SV_ClipDistance/SV_CullDistance.
  void requireCapabilityIfNecessary();

  /// Tries to access the builtin translated from the given HLSL semantic of the
  /// given index.
  ///
  /// If sigPoint indicates this is input, builtins will be read to compose a
  /// new temporary value of the correct type and writes to *value. Otherwise,
  /// the *value will be decomposed and writes to the builtins, unless
  /// noWriteBack is true, which means do not write back the value.
  ///
  /// If invocation (should only be used for HS) is not llvm::None, only
  /// accesses the element at the invocation offset in the gl_PerVeterx array.
  ///
  /// Emits SPIR-V instructions and returns true if we are accessing builtins
  /// belonging to gl_PerVertex. Does nothing and returns true if we are
  /// accessing builtins not in gl_PerVertex. Returns false if errors occurs.
  bool tryToAccess(hlsl::SigPoint::Kind sigPoint, hlsl::Semantic::Kind,
                   uint32_t semanticIndex, llvm::Optional<uint32_t> invocation,
                   uint32_t *value, bool noWriteBack);

  /// Similar to tryToAccess, but only used for the PointSize builtin.
  bool tryToAccessPointSize(hlsl::SigPoint::Kind sigPoint,
                            llvm::Optional<uint32_t> invocation,
                            uint32_t *value, bool noWriteBack);

private:
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N], SourceLocation loc) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(loc, diagId);
  }

  /// Creates a gl_PerVertex interface block variable. If arraySize is not zero,
  /// The created variable will be an array of gl_PerVertex of the given size.
  /// Otherwise, it will just be a plain struct.
  uint32_t createBlockVar(bool asInput, uint32_t arraySize);
  /// Creates a stand-alone Position builtin variable.
  uint32_t createPositionVar(bool asInput);
  /// Creates a stand-alone ClipDistance builtin variable.
  uint32_t createClipDistanceVar(bool asInput, uint32_t arraySize);
  /// Creates a stand-alone CullDistance builtin variable.
  uint32_t createCullDistanceVar(bool asInput, uint32_t arraySize);

  /// Emits SPIR-V instructions for reading the Position/PointSize builtin.
  uint32_t readPositionOrPointSize(bool isPosition) const;
  /// Emits SPIR-V instructions for reading the data starting from offset in
  /// the ClipDistance/CullDistance builtin. The data read will be transformed
  /// into the given type asType.
  uint32_t readClipCullArrayAsType(bool isClip, uint32_t offset,
                                   QualType asType) const;
  /// Emits SPIR-V instructions to read a field in gl_PerVertex.
  bool readField(hlsl::Semantic::Kind semanticKind, uint32_t semanticIndex,
                 uint32_t *value);

  /// Emits SPIR-V instructions for writing the Position/PointSize builtin.
  void writePositionOrPointSize(bool isPosition,
                                llvm::Optional<uint32_t> invocationId,
                                uint32_t value) const;
  /// Emits SPIR-V instructions for writing data into the ClipDistance/
  /// CullDistance builtin starting from offset. The value to be written is
  /// fromValue, whose type is fromType. Necessary transformations will be
  /// generated to make sure type correctness.
  void writeClipCullArrayFromType(llvm::Optional<uint32_t> invocationId,
                                  bool isClip, uint32_t offset,
                                  QualType fromType, uint32_t fromValue) const;
  /// Emits SPIR-V instructions to write a field in gl_PerVertex.
  bool writeField(hlsl::Semantic::Kind semanticKind, uint32_t semanticIndex,

                  llvm::Optional<uint32_t> invocationId, uint32_t *value);

  /// Internal implementation for recordClipCullDistanceDecl().
  bool doClipCullDistanceDecl(const DeclaratorDecl *decl, QualType type,
                              bool asInput);

private:
  using SemanticIndexToTypeMap = llvm::DenseMap<uint32_t, QualType>;
  using SemanticIndexToArrayOffsetMap = llvm::DenseMap<uint32_t, uint32_t>;

  const hlsl::ShaderModel &shaderModel;
  ASTContext &astContext;
  ModuleBuilder &theBuilder;
  TypeTranslator &typeTranslator;

  /// We can have Position, ClipDistance, and CullDistance either grouped (G)
  /// into the gl_PerVertex struct, or separated (S) as stand-alone variables.
  /// The following table shows for each shader stage, which one is used:
  ///
  /// ===== ===== ======
  /// Stage Input Output
  /// ===== ===== ======
  ///  VS     X     G
  ///  HS     G     G
  ///  DS     G     G
  ///  GS     G     S
  ///  PS     S     X
  /// ===== ===== ======
  ///
  /// Note that when we use separated variables, there is no extra arrayness.
  ///
  /// So depending on the shader stage, we may use one of the following set
  /// of variables to store <result-id>s of the variables:

  /// Indicates which set of variables are used.
  bool inIsGrouped, outIsGrouped;
  /// Input/output gl_PerVertex block variable if grouped.
  uint32_t inBlockVar, outBlockVar;
  /// Input/output ClipDistance/CullDistance variable if separated.
  uint32_t inClipVar, inCullVar;
  uint32_t outClipVar, outCullVar;

  /// The array size for the input/output gl_PerVertex block variabe.
  /// HS input and output, DS input, GS input has an additional level of
  /// arrayness. The array size is stored in this variable. Zero means
  /// the corresponding variable is a plain struct, not an array.
  uint32_t inArraySize, outArraySize;
  /// The array size of input/output ClipDistance/CullDistance float arrays.
  /// This is not the array size of the whole gl_PerVertex struct.
  uint32_t inClipArraySize, outClipArraySize;
  uint32_t inCullArraySize, outCullArraySize;

  /// We need to record all SV_ClipDistance/SV_CullDistance decls' types
  /// since we need to generate the necessary conversion instructions when
  /// accessing the ClipDistance/CullDistance builtins.
  SemanticIndexToTypeMap inClipType, outClipType;
  SemanticIndexToTypeMap inCullType, outCullType;
  /// We also need to keep track of all SV_ClipDistance/SV_CullDistance decls'
  /// offsets in the float array.
  SemanticIndexToArrayOffsetMap inClipOffset, outClipOffset;
  SemanticIndexToArrayOffsetMap inCullOffset, outCullOffset;
};

} // end namespace spirv
} // end namespace clang

#endif
