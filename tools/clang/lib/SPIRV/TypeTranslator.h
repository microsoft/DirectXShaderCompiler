//===--- TypeTranslator.h - AST type to SPIR-V type translator ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_TYPETRANSLATOR_H
#define LLVM_CLANG_LIB_SPIRV_TYPETRANSLATOR_H

#include <utility>

#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "dxc/Support/SPIRVOptions.h"
#include "llvm/ADT/Optional.h"

#include "SpirvEvalInfo.h"

namespace clang {
namespace spirv {

/// The class responsible to translate Clang frontend types into SPIR-V type
/// instructions.
///
/// SPIR-V type instructions generated during translation will be emitted to
/// the SPIR-V module builder passed into the constructor.
/// Warnings and errors during the translation will be reported to the
/// DiagnosticEngine passed into the constructor.
class TypeTranslator {
public:
  TypeTranslator(ASTContext &context, ModuleBuilder &builder,
                 DiagnosticsEngine &diag, const SpirvCodeGenOptions &opts)
      : astContext(context), theBuilder(builder), diags(diag),
        spirvOptions(opts) {}

  ~TypeTranslator() {
    // Perform any sanity checks.
    assert(intendedLiteralTypes.empty());
  }

  /// \brief Generates the corresponding SPIR-V type for the given Clang
  /// frontend type and returns the type's <result-id>. On failure, reports
  /// the error and returns 0. If decorateLayout is true, layout decorations
  /// (Offset, MatrixStride, ArrayStride, RowMajor, ColMajor) will be attached
  /// to the struct or array types. If layoutRule is not Void and type is a
  /// matrix or array of matrix type.
  ///
  /// The translation is recursive; all the types that the target type depends
  /// on will be generated and all with layout decorations (if decorateLayout
  /// is true).
  uint32_t translateType(QualType type,
                         SpirvLayoutRule layoutRule = SpirvLayoutRule::Void);

  /// \brief Generates the SPIR-V type for the counter associated with a
  /// {Append|Consume}StructuredBuffer: an OpTypeStruct with a single 32-bit
  /// integer value. This type will be decorated with BufferBlock.
  uint32_t getACSBufferCounter();

  /// \brief Returns true if the given type is a (RW)StructuredBuffer type.
  static bool isStructuredBuffer(QualType type);

  /// \brief Returns true if the given type is an AppendStructuredBuffer type.
  static bool isAppendStructuredBuffer(QualType type);

  /// \brief Returns true if the given type is a ConsumeStructuredBuffer type.
  static bool isConsumeStructuredBuffer(QualType type);

  /// \brief Returns true if the given type is a RW/Append/Consume
  /// StructuredBuffer type.
  static bool isRWAppendConsumeSBuffer(QualType type);

  /// \brief Returns true if the given type is the HLSL ByteAddressBufferType.
  static bool isByteAddressBuffer(QualType type);

  /// \brief Returns true if the given type is the HLSL RWByteAddressBufferType.
  static bool isRWByteAddressBuffer(QualType type);

  /// \brief Returns true if the given type is the HLSL (RW)StructuredBuffer,
  /// (RW)ByteAddressBuffer, or {Append|Consume}StructuredBuffer.
  static bool isAKindOfStructuredOrByteBuffer(QualType type);

  /// \brief Returns true if the given type is the HLSL (RW)StructuredBuffer,
  /// (RW)ByteAddressBuffer, {Append|Consume}StructuredBuffer, or a struct
  /// containing one of the above.
  static bool isOrContainsAKindOfStructuredOrByteBuffer(QualType type);

  /// \brief Returns true if the given type is or contains 16-bit type.
  bool isOrContains16BitType(QualType type);

  /// \brief Returns true if the given type is the HLSL Buffer type.
  static bool isBuffer(QualType type);

  /// \brief Returns true if the given type is the HLSL RWBuffer type.
  static bool isRWBuffer(QualType type);

  /// \brief Returns true if the given type is an HLSL Texture type.
  static bool isTexture(QualType);

  /// \brief Returns true if the given type is an HLSL Texture2DMS or
  /// Texture2DMSArray type.
  static bool isTextureMS(QualType);

  /// \brief Returns true if the given type is an HLSL RWTexture type.
  static bool isRWTexture(QualType);

  /// \brief Returns true if the given type is an HLSL sampler type.
  static bool isSampler(QualType);

  /// \brief Returns true if the given type is SubpassInput.
  static bool isSubpassInput(QualType);

  /// \brief Returns true if the given type is SubpassInputMS.
  static bool isSubpassInputMS(QualType);

  /// \brief Evluates the given type at the given bitwidth and returns the
  /// result-id for it. Panics if the given type is not a scalar or vector of
  /// float or integer type. For example: if QualType of an int4 and bitwidth of
  /// 64 is passed in, the result-id of a SPIR-V vector of size 4 of signed
  /// 64-bit integers is returned.
  /// Acceptable bitwidths are 16, 32, and 64.
  uint32_t getTypeWithCustomBitwidth(QualType type, uint32_t bitwidth);

  /// \brief Returns the realized bitwidth of the given type when represented in
  /// SPIR-V. Panics if the given type is not a scalar, a vector/matrix of float
  /// or integer, or an array of them. In case of vectors, it returns the
  /// realized SPIR-V bitwidth of the vector elements.
  uint32_t getElementSpirvBitwidth(QualType type);

  /// \brief Returns true if type is a row-major matrix, either with explicit
  /// attribute or implicit command-line option.
  bool isRowMajorMatrix(QualType type) const;

  /// \brief Returns true if the decl type is a non-floating-point matrix and
  /// the matrix is column major, or if it is an array/struct containing such
  /// matrices.
  bool isOrContainsNonFpColMajorMatrix(QualType type, const Decl *decl) const;

  /// \brief Returns true if the decl is of ConstantBuffer/TextureBuffer type.
  static bool isConstantTextureBuffer(const Decl *decl);

  /// \brief Returns true if the decl will have a SPIR-V resource type.
  ///
  /// Note that this function covers the following HLSL types:
  /// * ConstantBuffer/TextureBuffer
  /// * Various structured buffers
  /// * (RW)ByteAddressBuffer
  /// * SubpassInput(MS)
  static bool isResourceType(const ValueDecl *decl);

  /// \brief Returns true if the two types are the same scalar or vector type,
  /// regardless of constness and literalness.
  static bool isSameScalarOrVecType(QualType type1, QualType type2);

  /// \brief Returns true if the two types are the same type, regardless of
  /// constness and literalness.
  bool isSameType(QualType type1, QualType type2);

  /// \brief Returns true if the given type can use relaxed precision
  /// decoration. Integer and float types with lower than 32 bits can be
  /// operated on with a relaxed precision.
  static bool isRelaxedPrecisionType(QualType, const SpirvCodeGenOptions &);

  /// Returns true if the given type will be translated into a SPIR-V image,
  /// sampler or struct containing images or samplers.
  ///
  /// Note: legalization specific code
  static bool isOpaqueType(QualType type);

  /// Returns true if the given type will be translated into a array of SPIR-V
  /// images or samplers.
  static bool isOpaqueArrayType(QualType type);

  /// Returns true if the given type is a struct type who has an opaque field
  /// (in a recursive away).
  ///
  /// Note: legalization specific code
  static bool isOpaqueStructType(QualType tye);

  /// \brief Returns the the element type for the given scalar, vector, matrix,
  /// or array type. Returns empty QualType for other cases.
  QualType getElementType(QualType type);

  /// \brief Generates the corresponding SPIR-V vector type for the given Clang
  /// frontend matrix type's vector component and returns the <result-id>.
  ///
  /// This method will panic if the given matrix type is not a SPIR-V acceptable
  /// matrix type.
  QualType getComponentVectorType(QualType matrixType);
  uint32_t getComponentVectorTypeId(QualType matrixType);

  /// \brief Returns the QualType that has the same components as the source
  /// type, but with boolean element type. For instance, if the source type is a
  /// vector of 3 integers, returns the QualType for a vector of 3 booleans.
  /// Supports only scalars and vectors.
  QualType getBoolTypeWithSourceComponents(QualType srceType);
  /// \brief Returns the QualType that has the same components as the source
  /// type, but with 32-bit uint element type. For instance, if the source type
  /// is a vector of 3 booleans, returns the QualType for a vector of 3 uints.
  /// Supports only scalars and vectors.
  QualType getUintTypeWithSourceComponents(QualType srceType);

  /// \brief Returns true if all members in structType are of the same element
  /// type and can be fit into a 4-component vector. Writes element type and
  /// count to *elemType and *elemCount if not nullptr. Otherwise, emit errors
  /// explaining why not.
  bool canFitIntoOneRegister(QualType structType, QualType *elemType = nullptr,
                             uint32_t *elemCount = nullptr);

  /// \brief Returns the capability required for the given storage image type.
  /// Returns Capability::Max to mean no capability requirements.
  static spv::Capability getCapabilityForStorageImageReadWrite(QualType type);

  /// \brief Returns true if the given decl should be skipped when layouting
  /// a struct type.
  static bool shouldSkipInStructLayout(const Decl *decl);

  /// \brief Generates layout decorations (Offset, MatrixStride, RowMajor,
  /// ColMajor) for the given decl group.
  ///
  /// This method is not recursive; it only handles the top-level members/fields
  /// of the given Decl group. Besides, it does not handle ArrayStride, which
  /// according to the spec, must be attached to the array type itself instead
  /// of a struct member.
  llvm::SmallVector<const Decoration *, 4>
  getLayoutDecorations(const llvm::SmallVector<const Decl *, 4> &declGroup,
                       SpirvLayoutRule rule);

  /// \brief Returns how many sequential locations are consumed by a given type.
  uint32_t getLocationCount(QualType type);

  /// \brief Collects and returns all member/field declarations inside the given
  /// DeclContext. If it sees a NamespaceDecl, it recursively dives in and
  /// collects decls in the correct order.
  /// Utilizes collectDeclsInNamespace and collectDeclsInField private methods.
  llvm::SmallVector<const Decl *, 4>
  collectDeclsInDeclContext(const DeclContext *declContext);

private:
  /// \brief Appends any member/field decls found inside the given namespace
  /// into the give decl vector.
  void collectDeclsInNamespace(const NamespaceDecl *nsDecl,
                               llvm::SmallVector<const Decl *, 4> *decls);

  /// \brief Appends the given member/field decl into the given decl vector.
  void collectDeclsInField(const Decl *field,
                           llvm::SmallVector<const Decl *, 4> *decls);

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Returns true if the two types can be treated as the same scalar
  /// type, which means they have the same canonical type, regardless of
  /// constnesss and literalness.
  static bool canTreatAsSameScalarType(QualType type1, QualType type2);

  /// \brief Translates the given HLSL resource type into its SPIR-V
  /// instructions and returns the <result-id>. Returns 0 on failure.
  uint32_t translateResourceType(QualType type, SpirvLayoutRule rule);

  /// \brief For the given sampled type, returns the corresponding image format
  /// that can be used to create an image object.
  spv::ImageFormat translateSampledTypeToImageFormat(QualType type);

  /// \brief Aligns currentOffset properly to allow packing vectors in the HLSL
  /// way: using the element type's alignment as the vector alignment, as long
  /// as there is no improper straddle.
  /// fieldSize and fieldAlignment are the original size and alignment
  /// calculated without considering the HLSL vector relaxed rule.
  void alignUsingHLSLRelaxedLayout(QualType fieldType, uint32_t fieldSize,
                                   uint32_t fieldAlignment,
                                   uint32_t *currentOffset);

public:
  /// \brief Returns the alignment and size in bytes for the given type
  /// according to the given LayoutRule.

  /// If the type is an array/matrix type, writes the array/matrix stride to
  /// stride. If the type is a matrix.
  ///
  /// Note that the size returned is not exactly how many bytes the type
  /// will occupy in memory; rather it is used in conjunction with alignment
  /// to get the next available location (alignment + size), which means
  /// size contains post-paddings required by the given type.
  std::pair<uint32_t, uint32_t>
  getAlignmentAndSize(QualType type, SpirvLayoutRule rule, uint32_t *stride);

  /// \brief If a hint exists regarding the usage of literal types, it
  /// is returned. Otherwise, the given type itself is returned.
  /// The hint is the type on top of the intendedLiteralTypes stack. This is the
  /// type we suspect the literal under question should be interpreted as.
  QualType getIntendedLiteralType(QualType type);

  /// A RAII class for maintaining the intendedLiteralTypes stack.
  ///
  /// Instantiating an object of this class ensures that as long as the
  /// object lives, the hint lives in the TypeTranslator, and once the object is
  /// destroyed, the hint is automatically removed from the stack.
  class LiteralTypeHint {
  public:
    LiteralTypeHint(TypeTranslator &t, QualType ty);
    LiteralTypeHint(TypeTranslator &t);
    void setHint(QualType ty);
    ~LiteralTypeHint();

  private:
    static bool isLiteralType(QualType type);

  private:
    QualType type;
    TypeTranslator &translator;
  };

private:
  /// \brief Adds the given type to the intendedLiteralTypes stack. This will be
  /// used as a hint regarding usage of literal types.
  void pushIntendedLiteralType(QualType type);

  /// \brief Removes the type at the top of the intendedLiteralTypes stack.
  void popIntendedLiteralType();

  /// \brief Strip the attributes and typedefs fromthe given type and returns
  /// the desugared one. This method will update internal bookkeeping regarding
  /// matrix majorness.
  QualType desugarType(QualType type);

  ASTContext &astContext;
  ModuleBuilder &theBuilder;
  DiagnosticsEngine &diags;
  const SpirvCodeGenOptions &spirvOptions;

  /// \brief This is a stack which is used to track the intended usage type for
  /// literals. For example: while a floating literal is being visited, if the
  /// top of the stack is a float type, the literal should be evaluated as
  /// float; but if the top of the stack is a double type, the literal should be
  /// evaluated as a double.
  std::stack<QualType> intendedLiteralTypes;

  /// \brief A place to keep the matrix majorness attributes so that we can
  /// retrieve the information when really processing the desugared matrix type.
  /// This is needed because the majorness attribute is decorated on a
  /// TypedefType (i.e., floatMxN) of the real matrix type (i.e., matrix<elem,
  /// row, col>). When we reach the desugared matrix type, this information will
  /// already be gone.
  llvm::Optional<AttributedType::Kind> typeMatMajorAttr;
};

} // end namespace spirv
} // end namespace clang

#endif
