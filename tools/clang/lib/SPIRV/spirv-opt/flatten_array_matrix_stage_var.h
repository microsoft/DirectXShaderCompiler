//===--- flatten_array_matrix_stage_var.h - pass to flatten stage vars ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_SPIRV_OPT_FLATTEN_ARRAY_MATRIX_STAGE_VAR_H_
#define LLVM_CLANG_LIB_SPIRV_SPIRV_OPT_FLATTEN_ARRAY_MATRIX_STAGE_VAR_H_

#include <unordered_set>

#include "source/opt/pass.h"
#include "spirv-tools/optimizer.hpp"

namespace spvtools {
namespace opt {

// A struct for stage variable's location information including location,
// component, and extra arrayness. The extra arrayness is used for some
// stage variables of tessellation control shaders generated from HLSL hull
// shaders.
struct StageVariableLocationInfo {
  uint32_t location;
  uint32_t component;
  uint32_t extra_arrayness;
  bool is_input_var;

  bool operator==(const StageVariableLocationInfo &another) const {
    return another.location == location && another.component == component &&
           another.is_input_var == is_input_var;
  }
};

// A struct containing recursive flattened variables. If |variable| is nullptr,
// it recursively contains a vector of flattened variables. If |variable| is not
// nullptr, it is a single variable.
struct FlattenedVariables {
  FlattenedVariables() : variable(nullptr) {}

  std::vector<FlattenedVariables> flattened_variables;
  Instruction *variable;
};

class FlattenArrayMatrixStageVariable : public Pass {
public:
  // Hashing functor for StageVariableLocationInfo.
  struct StageVariableLocationInfoHash {
    size_t operator()(const StageVariableLocationInfo &info) const {
      return std::hash<uint32_t>()(info.location) ^
             std::hash<uint32_t>()(info.component) ^
             std::hash<uint32_t>()(static_cast<uint32_t>(info.is_input_var));
    }
  };

  using SetOfStageVariableLocationInfo =
      std::unordered_set<StageVariableLocationInfo,
                         StageVariableLocationInfoHash>;

  explicit FlattenArrayMatrixStageVariable(
      const std::vector<StageVariableLocationInfo> &stage_variable_locations)
      : stage_var_location_info_(stage_variable_locations.begin(),
                                 stage_variable_locations.end()) {}

  const char *name() const override {
    return "flatten-array-matrix-stage-variable";
  }
  Status Process() override;

  IRContext::Analysis GetPreservedAnalyses() override {
    return IRContext::kAnalysisDecorations | IRContext::kAnalysisDefUse |
           IRContext::kAnalysisConstants | IRContext::kAnalysisTypes;
  }

private:
  // Checks all stage variables that have Location and Component decorations
  // that are a part of |stage_var_location_info_| and collects the mapping from
  // stage variable ids to all stage variable location information.
  void CollectStageVariablesToFlatten(
      std::unordered_map<uint32_t, StageVariableLocationInfo>
          *stage_var_ids_to_stage_var_location_info);

  // Returns true if variable whose id is |var_id| that has Location decoration
  // with |location| is decorated by a Component decoration and the pair of
  // location and the component is one of |stage_var_location_info_|. Otherwise,
  // returns false. If the variable is a target, returns the stage variable
  // location information using |stage_var_location_info|.
  bool
  IsTargetStageVariable(uint32_t var_id, uint32_t location, bool is_input_var,
                        StageVariableLocationInfo *stage_var_location_info);

  // Returns the stage variable instruction whose result id is |stage_var_id|.
  Instruction *GetStageVariable(uint32_t stage_var_id);

  // Returns the type of |stage_var| as an instruction. If |stage_var| is not
  // an array or a matrix, returns nullptr. If |has_extra_arrayness| is true,
  // the stage variable has the extra arrayness.
  Instruction *GetTypeOfArrayOrMatrixStageVar(Instruction *stage_var,
                                              bool has_extra_arrayness);

  // Flattens a stage variable |stage_var| whose type is |stage_var_type| and
  // returns whether it succeeds or not. |extra_array_length| is the extra
  // arrayness of the stage variable.
  bool FlattenStageVariable(
      Instruction *stage_var, Instruction *stage_var_type,
      const StageVariableLocationInfo &stage_var_location_info);

  // Returns a vector of instructions that are users of |ptr| and satisfies
  // |condition| (i.e., `condition(user)` returns true).
  std::vector<Instruction *>
  GetUsersIf(Instruction *ptr,
             const std::function<bool(Instruction *)> &condition);

  // Creates flattened variables with the storage classe |storage_class| for the
  // stage variable whose type is |stage_var_type|. If |extra_array_length| is
  // not zero, adds the extra arrayness to all the flattened variables.
  FlattenedVariables
  CreateFlattenedStageVarsForReplacement(Instruction *stage_var_type,
                                         SpvStorageClass storage_class,
                                         uint32_t extra_array_length);

  // Creates flattened variables with the storage classe |storage_class| for the
  // stage variable whose type is OpTypeArray |stage_var_type|. If
  // |extra_array_length| is not zero, adds the extra arrayness to all the
  // flattened variables.
  FlattenedVariables
  CreateFlattenedStageVarsForArray(Instruction *stage_var_type,
                                   SpvStorageClass storage_class,
                                   uint32_t extra_array_length);

  // Creates flattened variables with the storage classe |storage_class| for the
  // stage variable whose type is OpTypeMatrix |stage_var_type|. If
  // |extra_array_length| is not zero, adds the extra arrayness to all the
  // flattened variables.
  FlattenedVariables
  CreateFlattenedStageVarsForMatrix(Instruction *stage_var_type,
                                    SpvStorageClass storage_class,
                                    uint32_t extra_array_length);

  // Recursively adds Location and Component decorations to variables in
  // |flattened_vars| with |location| and |component|. Increases |location| by
  // one after it actually adds Location and Component decorations for a
  // variable.
  void
  AddLocationAndComponentDecorations(const FlattenedVariables &flattened_vars,
                                     uint32_t *location, uint32_t component);

  // Replaces |stage_var| in the operands of instructions |stage_var_users|
  // with |flattened_stage_vars|. This is a recursive method and |indices|
  // is used to specify which recursive component of |stage_var| is replaced.
  // Returns composite construct instructions to be replaced with load
  // instructions of |stage_var_users| via |loads_to_composites|.
  bool ReplaceStageVars(
      Instruction *stage_var, std::vector<Instruction *> stage_var_users,
      const FlattenedVariables &flattened_stage_vars,
      std::vector<uint32_t> &indices, bool has_extra_arrayness,
      uint32_t extra_array_index,
      std::unordered_map<Instruction *, Instruction *> *loads_to_composites,
      std::unordered_map<Instruction *, Instruction *>
          *loads_for_access_chain_to_composites);

  // Replaces |stage_var| in the operands of instruction |stage_var_user|
  // with an instruction based on |flattened_var|. |indices| is recursive
  // indices for which recursive component of |stage_var| is replaced.
  // If |stage_var_user| is a load, returns the component value via
  // |loads_to_component_values|.
  bool ReplaceStageVar(Instruction *stage_var, Instruction *stage_var_user,
                       Instruction *flattened_var,
                       const std::vector<uint32_t> &indices,
                       bool has_extra_arrayness, uint32_t extra_array_index,
                       std::unordered_map<Instruction *, Instruction *>
                           *loads_to_component_values,
                       std::unordered_map<Instruction *, Instruction *>
                           *loads_for_access_chain_to_component_values);

  // Clones an annotation instruction |annotation_inst| and sets the target
  // operand of the new annotation instruction as |var_id|.
  void CloneAnnotationForVariable(Instruction *annotation_inst,
                                  uint32_t var_id);

  // Replaces the stage variable |stage_var| in the operands of the entry point
  // |entry_point| with |flattened_var_id|. If it cannot find |stage_var| from
  // the operands of the entry point |entry_point|, adds |flattened_var_id| as
  // an operand of the entry point |entry_point|.
  bool ReplaceStageVarInEntryPoint(Instruction *stage_var,
                                   Instruction *entry_point,
                                   uint32_t flattened_var_id);

  // Adds new instructions to store the |indices| th recursive component of
  // |store|'s Object operand in an access chain to the variable
  // |flattened_var| whose Indexes operands are the same as the ones of the
  // access chain instruction |access_chain|.
  //
  // Note that we have a strong assumption that |access_chain| has a single
  // index that is only for the extra arrayness of a HLSL hull shader. For other
  // shader types, |access_chain| will be nullptr.
  //
  // Before:
  //  %ptr = OpAccessChain %ptr_type %var %idx
  //  OpStore %ptr %value
  //
  // After:
  //  %flattened_ptr = OpAccessChain %flattened_ptr_type %flattened_var %idx
  //  %component = OpCompositeExtract %flattened_value_type %value <indices>
  //  OpStore %flattened_ptr %component
  void ReplaceStoreWithFlattenedVar(Instruction *access_chain,
                                    Instruction *store,
                                    const std::vector<uint32_t> &indices,
                                    Instruction *flattened_var,
                                    bool has_extra_arrayness,
                                    uint32_t extra_array_index);

  // Creates an access chain instruction whose Base operand is |var| and Indexes
  // operand is |index|. |component_type_id| is the id of the type instruction
  // that is the type of component. Inserts the new access chain before
  // |insert_before|.
  Instruction *CreateAccessChainWithIndex(uint32_t component_type_id,
                                          Instruction *var, uint32_t index,
                                          Instruction *insert_before);

  // Returns the pointee type of the type of variable |var|.
  uint32_t GetPointeeTypeIdOfVar(Instruction *var);

  // Replaces the access chain |access_chain| and its users with a new access
  // chain that points |flattened_var| as the Base operand having |indices| as
  // Indexes operands and users of the new access chain. When some of the users
  // are load instructions, returns the original load instruction to the
  // new instruction that loads a component of the original load value via
  // |loads_to_component_values|.
  void ReplaceAccessChainWithFlattenedVar(
      Instruction *access_chain, const std::vector<uint32_t> &indices,
      Instruction *flattened_var,
      std::unordered_map<Instruction *, Instruction *>
          *loads_to_component_values);

  // Assuming that |access_chain| is an access chain instruction whose Base
  // operand is |base_access_chain|, replaces the operands of |access_chain|
  // with operands of |base_access_chain| and Indexes operands of
  // |access_chain|.
  void UseBaseAccessChainForAccessChain(Instruction *access_chain,
                                        Instruction *base_access_chain);

  // Adds new instructions to replace the load instruction |load| that loads the
  // access chain |access_chain| with the instructions loading the components
  // using flattened variable |flattened_var|. |loads_to_component_values| is
  // used to return the mapping from |load| to the new load instruction for the
  // component.
  //
  // Note that we have a strong assumption that |access_chain| has a single
  // index that is only for the extra arrayness of a HLSL hull shader. For other
  // shader types, |access_chain| will be nullptr.
  //
  // Before:
  //  %ptr = OpAccessChain %ptr_type %var %idx
  //  %value = OpLoad %type %ptr
  //
  // After:
  //  %flattened_ptr = OpAccessChain %flattened_ptr_type %flattened_var %idx
  //  %flattened_value = OpLoad %flattened_type %flattened_ptr
  //  ..
  //  %value = OpCompositeConstruct %type %flattened_value ..
  void ReplaceLoadWithFlattenedVar(
      Instruction *access_chain, Instruction *load, Instruction *flattened_var,
      std::unordered_map<Instruction *, Instruction *>
          *loads_to_component_values,
      bool has_extra_arrayness, uint32_t extra_array_index);

  // Creates composite construct instructions for load instructions that are the
  // keys of |loads_to_component_values| if no such composite construct
  // instructions exist. Adds a component of the composite as an operand of the
  // created composite construct instruction. Each value of
  // |loads_to_component_values| is the component. Returns the created composite
  // construct instructions using |loads_to_composites|. |depth_to_component| is
  // the number of recursive access steps to get the component from the
  // composite.
  void AddComponentsToCompositesForLoads(
      const std::unordered_map<Instruction *, Instruction *>
          &loads_to_component_values,
      std::unordered_map<Instruction *, Instruction *> *loads_to_composites,
      uint32_t depth_to_component);

  // Creates a composite construct instruction for a component of the value of
  // instruction |load| in |depth_to_component| th recursive depth and inserts
  // it after |load|.
  Instruction *
  CreateCompositeConstructForComponentOfLoad(Instruction *load,
                                             uint32_t depth_to_component);

  // Creates a new access chain instruction that points to instruction |var|
  // whose type is the instruction with result id |var_type_id|. The new access
  // chain will have the same Indexes operands as |access_chain|. Returns the
  // type id of the component that is pointed by the new access chain via
  // |component_type_id|.
  Instruction *CreateAccessChainToVar(uint32_t var_type_id, Instruction *var,
                                      Instruction *access_chain,
                                      uint32_t *component_type_id);

  // Returns the result id of OpTypeArray instrunction whose Element Type
  // operand is |elem_type_id| and Length operand is |array_length|.
  uint32_t GetArrayType(uint32_t elem_type_id, uint32_t array_length);

  // Returns the result id of OpTypePointer instrunction whose Type
  // operand is |type_id| and Storage Class operand is |storage_class|.
  uint32_t GetPointerType(uint32_t type_id, SpvStorageClass storage_class);

  void KillInstructions(const std::vector<Instruction *> &insts);

  // A set of StageVariableLocationInfo that includes all locations and
  // components of stage variables to be flattened in this pass.
  SetOfStageVariableLocationInfo stage_var_location_info_;

  // A set of stage variable ids that were already removed from operands of the
  // entry point.
  std::unordered_set<uint32_t> stage_vars_removed_from_entry_point_operands_;

  // A mapping from ids of new composite construct instructions that load
  // instructions are replaced with to the recursive depth of the component of
  // load that the new component construct instruction is used for.
  std::unordered_map<uint32_t, uint32_t> composite_ids_to_component_depths;
};

} // namespace opt

Optimizer::PassToken CreateFlattenArrayMatrixStageVariablePass(
    const std::vector<opt::StageVariableLocationInfo>
        &stage_variable_locations);

} // namespace spvtools

#endif // LLVM_CLANG_LIB_SPIRV_SPIRV_OPT_FLATTEN_ARRAY_MATRIX_STAGE_VAR_H_
