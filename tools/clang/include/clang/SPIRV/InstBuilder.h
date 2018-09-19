//===-- InstBuilder.h - SPIR-V instruction builder --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// AUTOMATICALLY GENERATED from the SPIR-V JSON grammar:
//   spirv.core.grammar.json.
// DO NOT MODIFY!

#ifndef LLVM_CLANG_SPIRV_INSTBUILDER_H
#define LLVM_CLANG_SPIRV_INSTBUILDER_H

#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "spirv/unified1/spirv.hpp11"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

/// \brief SPIR-V word consumer.
using WordConsumer = std::function<void(std::vector<uint32_t> &&)>;

/// \brief A low-level SPIR-V instruction builder that generates SPIR-V words
/// directly. All generated SPIR-V words will be fed into the WordConsumer
/// passed in the constructor.
///
/// The methods of this builder reflects the layouts of the corresponding
/// SPIR-V instructions. For example, to construct an "OpMemoryModel Logical
/// Simple" instruction, just call InstBuilder::opMemoryModel(
/// spv::AddressingModel::Logical, spv::MemoryModel::Simple).
///
/// For SPIR-V instructions that may take additional parameters depending on
/// the value of some previous parameters, additional methods are provided to
/// "fix up" the instruction under building. For example, to construct an
/// "OpDecorate <target-id> ArrayStride 0" instruction, just call InstBuilder::
/// opDecorate(<target-id>, spv::Decoration::ArrayStride).literalInteger(0).
///
/// .x() is required to finalize the building and feed the result to the
/// consumer. On failure, if additional parameters are needed, the first missing
/// one will be reported by .x() via InstBuilder::Status.
class InstBuilder {
public:
  /// Status of instruction building.
  enum class Status : int32_t {
    Success = 0,
    NullConsumer = -1,
    NestedInst = -2,
    ZeroResultType = -3,
    ZeroResultId = -4,
    ExpectBuiltIn = -5,
    ExpectFPFastMathMode = -6,
    ExpectFPRoundingMode = -7,
    ExpectFunctionParameterAttribute = -8,
    ExpectIdRef = -9,
    ExpectLinkageType = -10,
    ExpectLiteralInteger = -11,
    ExpectLiteralString = -12
  };

  explicit InstBuilder(WordConsumer);

  // Disable copy constructor/assignment.
  InstBuilder(const InstBuilder &) = delete;
  InstBuilder &operator=(const InstBuilder &) = delete;

  // Allow move constructor/assignment.
  InstBuilder(InstBuilder &&that) = default;
  InstBuilder &operator=(InstBuilder &&that) = default;

  void setConsumer(WordConsumer);
  const WordConsumer &getConsumer() const;

  /// \brief Finalizes the building and feeds the generated SPIR-V words
  /// to the consumer.
  Status x();
  /// \brief Finalizes the building and returns the generated SPIR-V words.
  /// Returns an empty vector if errors happened during the construction.
  std::vector<uint32_t> take();

  /// \brief Clears the current instruction under building.
  void clear();

  // Instruction building methods.
  InstBuilder &opSourceContinued(llvm::StringRef continued_source);
  InstBuilder &opSource(spv::SourceLanguage source_language, uint32_t version,
                        llvm::Optional<uint32_t> file,
                        llvm::Optional<llvm::StringRef> source);
  InstBuilder &opName(uint32_t target, std::string name);
  InstBuilder &opMemberName(uint32_t type, uint32_t member, std::string name);
  InstBuilder &opString(uint32_t result_id, std::string string);
  InstBuilder &opLine(uint32_t file, uint32_t line, uint32_t column);
  InstBuilder &opExtension(std::string name);
  InstBuilder &opExtInstImport(uint32_t result_id, std::string name);
  InstBuilder &opExtInst(uint32_t result_type, uint32_t result_id, uint32_t set,
                         uint32_t instruction,
                         llvm::ArrayRef<uint32_t> operand_1_operand_2_);
  InstBuilder &opMemoryModel(spv::AddressingModel addressing_model,
                             spv::MemoryModel memory_model);
  InstBuilder &opEntryPoint(spv::ExecutionModel execution_model,
                            uint32_t entry_point, std::string name,
                            llvm::ArrayRef<uint32_t> interface);
  InstBuilder &opExecutionMode(uint32_t entry_point, spv::ExecutionMode mode);
  InstBuilder &opCapability(spv::Capability capability);
  InstBuilder &opTypeVoid(uint32_t result_id);
  InstBuilder &opTypeBool(uint32_t result_id);
  InstBuilder &opTypeInt(uint32_t result_id, uint32_t width,
                         uint32_t signedness);
  InstBuilder &opTypeFloat(uint32_t result_id, uint32_t width);
  InstBuilder &opTypeVector(uint32_t result_id, uint32_t component_type,
                            uint32_t component_count);
  InstBuilder &opTypeMatrix(uint32_t result_id, uint32_t column_type,
                            uint32_t column_count);
  InstBuilder &
  opTypeImage(uint32_t result_id, uint32_t sampled_type, spv::Dim dim,
              uint32_t depth, uint32_t arrayed, uint32_t ms, uint32_t sampled,
              spv::ImageFormat image_format,
              llvm::Optional<spv::AccessQualifier> access_qualifier);
  InstBuilder &opTypeSampler(uint32_t result_id);
  InstBuilder &opTypeSampledImage(uint32_t result_id, uint32_t image_type);
  InstBuilder &opTypeArray(uint32_t result_id, uint32_t element_type,
                           uint32_t length);
  InstBuilder &opTypeRuntimeArray(uint32_t result_id, uint32_t element_type);
  InstBuilder &
  opTypeStruct(uint32_t result_id,
               llvm::ArrayRef<uint32_t> member_0_type_member_1_type_);
  InstBuilder &opTypeOpaque(uint32_t result_id,
                            std::string the_name_of_the_opaque_type);
  InstBuilder &opTypePointer(uint32_t result_id,
                             spv::StorageClass storage_class, uint32_t type);
  InstBuilder &
  opTypeFunction(uint32_t result_id, uint32_t return_type,
                 llvm::ArrayRef<uint32_t> parameter_0_type_parameter_1_type_);
  InstBuilder &opTypeEvent(uint32_t result_id);
  InstBuilder &opTypeDeviceEvent(uint32_t result_id);
  InstBuilder &opTypeReserveId(uint32_t result_id);
  InstBuilder &opTypeQueue(uint32_t result_id);
  InstBuilder &opTypePipe(uint32_t result_id, spv::AccessQualifier qualifier);
  InstBuilder &opTypeForwardPointer(uint32_t pointer_type,
                                    spv::StorageClass storage_class);
  InstBuilder &opConstantTrue(uint32_t result_type, uint32_t result_id);
  InstBuilder &opConstantFalse(uint32_t result_type, uint32_t result_id);
  InstBuilder &opConstantComposite(uint32_t result_type, uint32_t result_id,
                                   llvm::ArrayRef<uint32_t> constituents);
  InstBuilder &opConstantNull(uint32_t result_type, uint32_t result_id);
  InstBuilder &opSpecConstantTrue(uint32_t result_type, uint32_t result_id);
  InstBuilder &opSpecConstantFalse(uint32_t result_type, uint32_t result_id);
  InstBuilder &opSpecConstantComposite(uint32_t result_type, uint32_t result_id,
                                       llvm::ArrayRef<uint32_t> constituents);
  InstBuilder &opFunction(uint32_t result_type, uint32_t result_id,
                          spv::FunctionControlMask function_control,
                          uint32_t function_type);
  InstBuilder &opFunctionParameter(uint32_t result_type, uint32_t result_id);
  InstBuilder &opFunctionEnd();
  InstBuilder &opFunctionCall(uint32_t result_type, uint32_t result_id,
                              uint32_t function,
                              llvm::ArrayRef<uint32_t> argument_0_argument_1_);
  InstBuilder &opVariable(uint32_t result_type, uint32_t result_id,
                          spv::StorageClass storage_class,
                          llvm::Optional<uint32_t> initializer);
  InstBuilder &opImageTexelPointer(uint32_t result_type, uint32_t result_id,
                                   uint32_t image, uint32_t coordinate,
                                   uint32_t sample);
  InstBuilder &opLoad(uint32_t result_type, uint32_t result_id,
                      uint32_t pointer,
                      llvm::Optional<spv::MemoryAccessMask> memory_access);
  InstBuilder &opStore(uint32_t pointer, uint32_t object,
                       llvm::Optional<spv::MemoryAccessMask> memory_access);
  InstBuilder &opAccessChain(uint32_t result_type, uint32_t result_id,
                             uint32_t base, llvm::ArrayRef<uint32_t> indexes);
  InstBuilder &opDecorate(uint32_t target, spv::Decoration decoration);
  InstBuilder &opMemberDecorate(uint32_t structure_type, uint32_t member,
                                spv::Decoration decoration);
  InstBuilder &opDecorationGroup(uint32_t result_id);
  InstBuilder &opGroupDecorate(uint32_t decoration_group,
                               llvm::ArrayRef<uint32_t> targets);
  InstBuilder &
  opGroupMemberDecorate(uint32_t decoration_group,
                        llvm::ArrayRef<std::pair<uint32_t, uint32_t>> targets);
  InstBuilder &opVectorShuffle(uint32_t result_type, uint32_t result_id,
                               uint32_t vector_1, uint32_t vector_2,
                               llvm::ArrayRef<uint32_t> components);
  InstBuilder &opCompositeConstruct(uint32_t result_type, uint32_t result_id,
                                    llvm::ArrayRef<uint32_t> constituents);
  InstBuilder &opCompositeExtract(uint32_t result_type, uint32_t result_id,
                                  uint32_t composite,
                                  llvm::ArrayRef<uint32_t> indexes);
  InstBuilder &opCompositeInsert(uint32_t result_type, uint32_t result_id,
                                 uint32_t object, uint32_t composite,
                                 llvm::ArrayRef<uint32_t> indexes);
  InstBuilder &opTranspose(uint32_t result_type, uint32_t result_id,
                           uint32_t matrix);
  InstBuilder &opSampledImage(uint32_t result_type, uint32_t result_id,
                              uint32_t image, uint32_t sampler);
  InstBuilder &opImageSampleImplicitLod(
      uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
      uint32_t coordinate,
      llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &opImageSampleExplicitLod(uint32_t result_type,
                                        uint32_t result_id,
                                        uint32_t sampled_image,
                                        uint32_t coordinate,
                                        spv::ImageOperandsMask image_operands);
  InstBuilder &opImageSampleDrefImplicitLod(
      uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
      uint32_t coordinate, uint32_t dref,
      llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageSampleDrefExplicitLod(uint32_t result_type, uint32_t result_id,
                               uint32_t sampled_image, uint32_t coordinate,
                               uint32_t dref,
                               spv::ImageOperandsMask image_operands);
  InstBuilder &
  opImageFetch(uint32_t result_type, uint32_t result_id, uint32_t image,
               uint32_t coordinate,
               llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageGather(uint32_t result_type, uint32_t result_id,
                uint32_t sampled_image, uint32_t coordinate, uint32_t component,
                llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageDrefGather(uint32_t result_type, uint32_t result_id,
                    uint32_t sampled_image, uint32_t coordinate, uint32_t dref,
                    llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageRead(uint32_t result_type, uint32_t result_id, uint32_t image,
              uint32_t coordinate,
              llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageWrite(uint32_t image, uint32_t coordinate, uint32_t texel,
               llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &opImageQueryFormat(uint32_t result_type, uint32_t result_id,
                                  uint32_t image);
  InstBuilder &opImageQueryOrder(uint32_t result_type, uint32_t result_id,
                                 uint32_t image);
  InstBuilder &opImageQuerySizeLod(uint32_t result_type, uint32_t result_id,
                                   uint32_t image, uint32_t level_of_detail);
  InstBuilder &opImageQuerySize(uint32_t result_type, uint32_t result_id,
                                uint32_t image);
  InstBuilder &opImageQueryLod(uint32_t result_type, uint32_t result_id,
                               uint32_t sampled_image, uint32_t coordinate);
  InstBuilder &opImageQueryLevels(uint32_t result_type, uint32_t result_id,
                                  uint32_t image);
  InstBuilder &opImageQuerySamples(uint32_t result_type, uint32_t result_id,
                                   uint32_t image);
  InstBuilder &opSelect(uint32_t result_type, uint32_t result_id,
                        uint32_t condition, uint32_t object_1,
                        uint32_t object_2);
  InstBuilder &opBitFieldInsert(uint32_t result_type, uint32_t result_id,
                                uint32_t base, uint32_t insert, uint32_t offset,
                                uint32_t count);
  InstBuilder &opBitFieldSExtract(uint32_t result_type, uint32_t result_id,
                                  uint32_t base, uint32_t offset,
                                  uint32_t count);
  InstBuilder &opBitFieldUExtract(uint32_t result_type, uint32_t result_id,
                                  uint32_t base, uint32_t offset,
                                  uint32_t count);
  InstBuilder &opBitReverse(uint32_t result_type, uint32_t result_id,
                            uint32_t base);
  InstBuilder &opBitCount(uint32_t result_type, uint32_t result_id,
                          uint32_t base);
  InstBuilder &opDPdx(uint32_t result_type, uint32_t result_id, uint32_t p);
  InstBuilder &opDPdy(uint32_t result_type, uint32_t result_id, uint32_t p);
  InstBuilder &opFwidth(uint32_t result_type, uint32_t result_id, uint32_t p);
  InstBuilder &opDPdxFine(uint32_t result_type, uint32_t result_id, uint32_t p);
  InstBuilder &opDPdyFine(uint32_t result_type, uint32_t result_id, uint32_t p);
  InstBuilder &opFwidthFine(uint32_t result_type, uint32_t result_id,
                            uint32_t p);
  InstBuilder &opDPdxCoarse(uint32_t result_type, uint32_t result_id,
                            uint32_t p);
  InstBuilder &opDPdyCoarse(uint32_t result_type, uint32_t result_id,
                            uint32_t p);
  InstBuilder &opFwidthCoarse(uint32_t result_type, uint32_t result_id,
                              uint32_t p);
  InstBuilder &opEmitVertex();
  InstBuilder &opEndPrimitive();
  InstBuilder &opControlBarrier(uint32_t execution, uint32_t memory,
                                uint32_t semantics);
  InstBuilder &opMemoryBarrier(uint32_t memory, uint32_t semantics);
  InstBuilder &opAtomicExchange(uint32_t result_type, uint32_t result_id,
                                uint32_t pointer, uint32_t scope,
                                uint32_t semantics, uint32_t value);
  InstBuilder &opAtomicCompareExchange(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t equal, uint32_t unequal,
                                       uint32_t value, uint32_t comparator);
  InstBuilder &opLoopMerge(uint32_t merge_block, uint32_t continue_target,
                           spv::LoopControlMask loop_control);
  InstBuilder &opSelectionMerge(uint32_t merge_block,
                                spv::SelectionControlMask selection_control);
  InstBuilder &opLabel(uint32_t result_id);
  InstBuilder &opBranch(uint32_t target_label);
  InstBuilder &opBranchConditional(uint32_t condition, uint32_t true_label,
                                   uint32_t false_label,
                                   llvm::ArrayRef<uint32_t> branch_weights);
  InstBuilder &opSwitch(uint32_t selector, uint32_t default_target,
                        llvm::ArrayRef<std::pair<uint32_t, uint32_t>> target);
  InstBuilder &opKill();
  InstBuilder &opReturn();
  InstBuilder &opReturnValue(uint32_t value);
  InstBuilder &opUnreachable();
  InstBuilder &opImageSparseSampleImplicitLod(
      uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
      uint32_t coordinate,
      llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageSparseSampleExplicitLod(uint32_t result_type, uint32_t result_id,
                                 uint32_t sampled_image, uint32_t coordinate,
                                 spv::ImageOperandsMask image_operands);
  InstBuilder &opImageSparseSampleDrefImplicitLod(
      uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
      uint32_t coordinate, uint32_t dref,
      llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageSparseSampleDrefExplicitLod(uint32_t result_type, uint32_t result_id,
                                     uint32_t sampled_image,
                                     uint32_t coordinate, uint32_t dref,
                                     spv::ImageOperandsMask image_operands);
  InstBuilder &
  opImageSparseFetch(uint32_t result_type, uint32_t result_id, uint32_t image,
                     uint32_t coordinate,
                     llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &
  opImageSparseGather(uint32_t result_type, uint32_t result_id,
                      uint32_t sampled_image, uint32_t coordinate,
                      uint32_t component,
                      llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &opImageSparseDrefGather(
      uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
      uint32_t coordinate, uint32_t dref,
      llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &opImageSparseTexelsResident(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t resident_code);
  InstBuilder &
  opImageSparseRead(uint32_t result_type, uint32_t result_id, uint32_t image,
                    uint32_t coordinate,
                    llvm::Optional<spv::ImageOperandsMask> image_operands);
  InstBuilder &opModuleProcessed(std::string process);
  InstBuilder &opExecutionModeId(uint32_t entry_point, spv::ExecutionMode mode);
  InstBuilder &opDecorateId(uint32_t target, spv::Decoration decoration);
  InstBuilder &opDecorateStringGOOGLE(uint32_t target,
                                      spv::Decoration decoration);
  InstBuilder &opMemberDecorateStringGOOGLE(uint32_t struct_type,
                                            uint32_t member,
                                            spv::Decoration decoration);

  // All-in-one methods for creating unary and binary operations.
  InstBuilder &unaryOp(spv::Op op, uint32_t result_type, uint32_t result_id,
                       uint32_t operand);
  InstBuilder &binaryOp(spv::Op op, uint32_t result_type, uint32_t result_id,
                        uint32_t lhs, uint32_t rhs);
  InstBuilder &specConstantBinaryOp(spv::Op op, uint32_t result_type,
                                    uint32_t result_id, uint32_t lhs,
                                    uint32_t rhs);
  InstBuilder &atomicOp(spv::Op op, uint32_t result_type, uint32_t result_id,
                        uint32_t pointer, uint32_t scope, uint32_t semantics,
                        uint32_t value);

  // All-in-one methods for creating OpGroupNonUniform* operations.
  InstBuilder &groupNonUniformOp(spv::Op op, uint32_t result_type,
                                 uint32_t result_id, uint32_t exec_scope);
  InstBuilder &groupNonUniformUnaryOp(
      spv::Op op, uint32_t result_type, uint32_t result_id, uint32_t exec_scope,
      llvm::Optional<spv::GroupOperation> groupOp, uint32_t operand);
  InstBuilder &groupNonUniformBinaryOp(spv::Op op, uint32_t result_type,
                                       uint32_t result_id, uint32_t exec_scope,
                                       uint32_t operand1, uint32_t operand2);

  // Methods for building constants.
  InstBuilder &opConstant(uint32_t result_type, uint32_t result_id,
                          uint32_t value);
  InstBuilder &opSpecConstant(uint32_t result_type, uint32_t result_id,
                              uint32_t value);

  // All-in-one method for creating different types of OpImageSample*.
  InstBuilder &
  opImageSample(uint32_t result_type, uint32_t result_id,
                uint32_t sampled_image, uint32_t coordinate, uint32_t dref,
                llvm::Optional<spv::ImageOperandsMask> image_operands,
                bool is_explicit, bool is_sparse);

  // All-in-one method for creating different types of
  // OpImageRead*/OpImageFetch*.
  InstBuilder &
  opImageFetchRead(uint32_t result_type, uint32_t result_id, uint32_t image,
                   uint32_t coordinate,
                   llvm::Optional<spv::ImageOperandsMask> image_operands,
                   bool is_fetch, bool is_sparse);

  // Methods for supplying additional parameters.
  InstBuilder &fPFastMathMode(spv::FPFastMathModeMask);
  InstBuilder &fPRoundingMode(spv::FPRoundingMode);
  InstBuilder &linkageType(spv::LinkageType);
  InstBuilder &functionParameterAttribute(spv::FunctionParameterAttribute);
  InstBuilder &builtIn(spv::BuiltIn);
  InstBuilder &idRef(uint32_t);
  InstBuilder &literalInteger(uint32_t);
  InstBuilder &literalString(std::string);

private:
  enum class OperandKind {
    BuiltIn,
    FPFastMathMode,
    FPRoundingMode,
    FunctionParameterAttribute,
    IdRef,
    LinkageType,
    LiteralInteger,
    LiteralString
  };

  void encodeImageOperands(spv::ImageOperandsMask value);
  void encodeLoopControl(spv::LoopControlMask value);
  void encodeMemoryAccess(spv::MemoryAccessMask value);
  void encodeExecutionMode(spv::ExecutionMode value);
  void encodeDecoration(spv::Decoration value);
  void encodeString(llvm::StringRef value);

  WordConsumer TheConsumer;
  std::vector<uint32_t> TheInst;       ///< The instruction under construction.
  std::deque<OperandKind> Expectation; ///< Expected additional parameters.
  Status TheStatus;                    ///< Current building status.
};

} // end namespace spirv
} // end namespace clang

#endif
