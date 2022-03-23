// Copyright (c) 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "flatten_array_matrix_stage_var.h"

#include <string>

#include "source/opt/build_module.h"
#include "source/opt/decoration_manager.h"
#include "source/opt/def_use_manager.h"
#include "source/opt/function.h"
#include "source/opt/log.h"
#include "source/opt/pass_manager.h"
#include "source/opt/type_manager.h"
#include "source/spirv_optimizer_options.h"
#include "source/util/make_unique.h"

const static uint32_t kOpDecorateTargetInOperandIndex = 0;
const static uint32_t kOpDecorateDecorationInOperandIndex = 1;
const static uint32_t kOpDecorateLiteralInOperandIndex = 2;
const static uint32_t kOpVariableStorageClassInOperandIndex = 0;
const static uint32_t kOpTypeArrayElemTypeInOperandIndex = 0;
const static uint32_t kOpTypeArrayLengthInOperandIndex = 1;
const static uint32_t kOpTypeMatrixColCountInOperandIndex = 1;
const static uint32_t kOpTypeMatrixColTypeInOperandIndex = 0;
const static uint32_t kOpTypePtrTypeInOperandIndex = 1;
const static uint32_t kOpConstantValueInOperandIndex = 0;

namespace spvtools {

struct Optimizer::PassToken::Impl {
  Impl(std::unique_ptr<opt::Pass> p) : pass(std::move(p)) {}

  std::unique_ptr<opt::Pass> pass; // Internal implementation pass.
};

Optimizer::PassToken CreateFlattenArrayMatrixStageVariablePass(
    const std::vector<opt::StageVariableLocationInfo>
        &stage_variable_locations) {
  return MakeUnique<Optimizer::PassToken::Impl>(
      MakeUnique<opt::FlattenArrayMatrixStageVariable>(
          stage_variable_locations));
}

namespace opt {
namespace {

// Get the length of the OpTypeArray |array_type|.
uint32_t GetArrayLength(analysis::DefUseManager *def_use_mgr,
                        Instruction *array_type) {
  assert(array_type->opcode() == SpvOpTypeArray);
  uint32_t const_int_id =
      array_type->GetSingleWordInOperand(kOpTypeArrayLengthInOperandIndex);
  Instruction *array_length_inst = def_use_mgr->GetDef(const_int_id);
  assert(array_length_inst->opcode() == SpvOpConstant);
  return array_length_inst->GetSingleWordInOperand(
      kOpConstantValueInOperandIndex);
}

// Get the element type instruction of the OpTypeArray |array_type|.
Instruction *GetArrayElementType(analysis::DefUseManager *def_use_mgr,
                                 Instruction *array_type) {
  assert(array_type->opcode() == SpvOpTypeArray);
  uint32_t elem_type_id =
      array_type->GetSingleWordInOperand(kOpTypeArrayElemTypeInOperandIndex);
  return def_use_mgr->GetDef(elem_type_id);
}

// Get the column type instruction of the OpTypeMatrix |matrix_type|.
Instruction *GetMatrixColumnType(analysis::DefUseManager *def_use_mgr,
                                 Instruction *matrix_type) {
  assert(matrix_type->opcode() == SpvOpTypeMatrix);
  uint32_t column_type_id =
      matrix_type->GetSingleWordInOperand(kOpTypeMatrixColTypeInOperandIndex);
  return def_use_mgr->GetDef(column_type_id);
}

// Returns the result id of the component type instruction of OpTypeMatrix or
// OpTypeArray in |depth_to_component| th recursive depth whose result id is
// |type_id|.
uint32_t FindComponentTypeOfArrayMatrix(analysis::DefUseManager *def_use_mgr,
                                        uint32_t type_id,
                                        uint32_t depth_to_component) {
  if (depth_to_component == 0)
    return type_id;

  Instruction *type_inst = def_use_mgr->GetDef(type_id);
  if (type_inst->opcode() == SpvOpTypeArray) {
    uint32_t elem_type_id =
        type_inst->GetSingleWordInOperand(kOpTypeArrayElemTypeInOperandIndex);
    return FindComponentTypeOfArrayMatrix(def_use_mgr, elem_type_id,
                                          depth_to_component - 1);
  }

  assert(type_inst->opcode() == SpvOpTypeMatrix);
  uint32_t column_type_id =
      type_inst->GetSingleWordInOperand(kOpTypeMatrixColTypeInOperandIndex);
  return FindComponentTypeOfArrayMatrix(def_use_mgr, column_type_id,
                                        depth_to_component - 1);
}

// Creates an OpDecorate instruction whose Target is |var_id| and Decoration is
// |decoration|. Adds |literal| as an extra operand of the instruction.
void CreateLocationDecoration(analysis::DecorationManager *decoration_mgr,
                              uint32_t var_id, SpvDecoration decoration,
                              uint32_t literal) {
  std::vector<Operand> operands({
      {spv_operand_type_t::SPV_OPERAND_TYPE_ID, {var_id}},
      {spv_operand_type_t::SPV_OPERAND_TYPE_DECORATION, {decoration}},
      {spv_operand_type_t::SPV_OPERAND_TYPE_LITERAL_INTEGER, {literal}},
  });
  decoration_mgr->AddDecoration(SpvOpDecorate, std::move(operands));
}

void ReplaceLoadWithCompositeConstruct(
    IRContext *context, const std::unordered_map<Instruction *, Instruction *>
                            &loads_to_composites) {
  for (const auto &load_and_composite : loads_to_composites) {
    Instruction *load = load_and_composite.first;
    Instruction *composite_construct = load_and_composite.second;

    std::vector<Instruction *> users;
    context->get_def_use_mgr()->ForEachUse(
        load, [&users, composite_construct](Instruction *user, uint32_t index) {
          user->GetOperand(index).words[0] = composite_construct->result_id();
          users.push_back(user);
        });

    for (Instruction *user : users)
      context->get_def_use_mgr()->AnalyzeInstUse(user);
  }
}

} // namespace

bool FlattenArrayMatrixStageVariable::IsTargetStageVariable(
    uint32_t var_id, uint32_t location, bool is_input_var,
    StageVariableLocationInfo *stage_var_location_info) {
  // It has both Location and Component decorations.
  bool has_component = false;
  if (!context()->get_decoration_mgr()->WhileEachDecoration(
          var_id, SpvDecorationComponent,
          [this, &location, &is_input_var, &has_component,
           stage_var_location_info](const Instruction &decoration_inst) {
            has_component = true;
            uint32_t component = decoration_inst.GetSingleWordInOperand(
                kOpDecorateLiteralInOperandIndex);
            auto stage_var_location_itr = stage_var_location_info_.find(
                {location, component, 0, is_input_var});
            if (stage_var_location_itr == stage_var_location_info_.end())
              return true;
            *stage_var_location_info = *stage_var_location_itr;
            return false;
          })) {
    return true;
  }
  if (has_component)
    return false;

  // It has only Location decoration.
  auto stage_var_location_itr =
      stage_var_location_info_.find({location, 0, 0, is_input_var});
  if (stage_var_location_itr == stage_var_location_info_.end())
    return false;
  *stage_var_location_info = *stage_var_location_itr;
  return true;
}

void FlattenArrayMatrixStageVariable::CollectStageVariablesToFlatten(
    std::unordered_map<uint32_t, StageVariableLocationInfo>
        *stage_var_ids_to_stage_var_location_info) {
  for (auto &annotation : get_module()->annotations()) {
    if (annotation.opcode() != SpvOpDecorate)
      continue;
    if (annotation.GetSingleWordInOperand(
            kOpDecorateDecorationInOperandIndex) != SpvDecorationLocation) {
      continue;
    }
    uint32_t var_id =
        annotation.GetSingleWordInOperand(kOpDecorateTargetInOperandIndex);
    uint32_t location =
        annotation.GetSingleWordInOperand(kOpDecorateLiteralInOperandIndex);

    Instruction *var = context()->get_def_use_mgr()->GetDef(var_id);
    SpvStorageClass storage_class = static_cast<SpvStorageClass>(
        var->GetSingleWordInOperand(kOpVariableStorageClassInOperandIndex));
    assert(storage_class == SpvStorageClassInput ||
           storage_class == SpvStorageClassOutput);

    StageVariableLocationInfo stage_var_location_info;
    if (!IsTargetStageVariable(var_id, location,
                               storage_class == SpvStorageClassInput,
                               &stage_var_location_info)) {
      continue;
    }

    stage_var_ids_to_stage_var_location_info->insert(
        {var_id, stage_var_location_info});
  }
}

void FlattenArrayMatrixStageVariable::KillInstructions(
    const std::vector<Instruction *> &insts) {
  for (Instruction *inst : insts) {
    if (inst->opcode() == SpvOpEntryPoint) {
      continue;
    }
    if (inst->opcode() != SpvOpAccessChain) {
      context()->KillInst(inst);
      continue;
    }
    std::vector<Instruction *> users =
        GetUsersIf(inst, [](Instruction *) { return true; });
    KillInstructions(users);
    context()->KillInst(inst);
  }
}

bool FlattenArrayMatrixStageVariable::FlattenStageVariable(
    Instruction *stage_var, Instruction *stage_var_type,
    const StageVariableLocationInfo &stage_var_location_info) {
  std::vector<Instruction *> location_component_decorations;
  std::vector<Instruction *> stage_var_users = GetUsersIf(
      stage_var, [&location_component_decorations](Instruction *user) {
        if (user->opcode() != SpvOpDecorate)
          return true;
        uint32_t decoration =
            user->GetSingleWordInOperand(kOpDecorateDecorationInOperandIndex);
        if (decoration == SpvDecorationLocation ||
            decoration == SpvDecorationComponent) {
          location_component_decorations.push_back(user);
          return false;
        }
        return true;
      });

  SpvStorageClass storage_class = static_cast<SpvStorageClass>(
      stage_var->GetSingleWordInOperand(kOpVariableStorageClassInOperandIndex));

  FlattenedVariables flattened_stage_vars =
      CreateFlattenedStageVarsForReplacement(
          stage_var_type, storage_class,
          stage_var_location_info.extra_arrayness);

  uint32_t location = stage_var_location_info.location;
  uint32_t component = stage_var_location_info.component;
  AddLocationAndComponentDecorations(flattened_stage_vars, &location,
                                     component);

  std::vector<uint32_t> indices;
  std::unordered_map<Instruction *, Instruction *> loads_to_composites;
  std::unordered_map<Instruction *, Instruction *>
      loads_for_access_chain_to_composites;
  if (stage_var_location_info.extra_arrayness != 0) {
    for (uint32_t index = 0; index < stage_var_location_info.extra_arrayness;
         ++index) {
      std::unordered_map<Instruction *, Instruction *>
          loads_to_component_values;
      if (!ReplaceStageVars(stage_var, stage_var_users, flattened_stage_vars,
                            indices, true, index, &loads_to_component_values,
                            &loads_for_access_chain_to_composites)) {
        return false;
      }
      AddComponentsToCompositesForLoads(loads_to_component_values,
                                        &loads_to_composites, 0);
    }
  } else if (!ReplaceStageVars(stage_var, stage_var_users, flattened_stage_vars,
                               indices, false, 0, &loads_to_composites,
                               &loads_for_access_chain_to_composites)) {
    return false;
  }

  ReplaceLoadWithCompositeConstruct(context(), loads_to_composites);
  ReplaceLoadWithCompositeConstruct(context(),
                                    loads_for_access_chain_to_composites);

  KillInstructions(location_component_decorations);
  KillInstructions(stage_var_users);
  context()->KillInst(stage_var);
  return true;
}

void FlattenArrayMatrixStageVariable::AddLocationAndComponentDecorations(
    const FlattenedVariables &flattened_vars, uint32_t *location,
    uint32_t component) {
  if (flattened_vars.variable != nullptr) {
    CreateLocationDecoration(context()->get_decoration_mgr(),
                             flattened_vars.variable->result_id(),
                             SpvDecorationLocation, *location);
    CreateLocationDecoration(context()->get_decoration_mgr(),
                             flattened_vars.variable->result_id(),
                             SpvDecorationComponent, component);
    ++(*location);
    return;
  }
  for (const auto &flattened_var : flattened_vars.flattened_variables) {
    AddLocationAndComponentDecorations(flattened_var, location, component);
  }
}

bool FlattenArrayMatrixStageVariable::ReplaceStageVars(
    Instruction *stage_var, std::vector<Instruction *> stage_var_users,
    const FlattenedVariables &flattened_stage_vars,
    std::vector<uint32_t> &indices, bool has_extra_arrayness,
    uint32_t extra_array_index,
    std::unordered_map<Instruction *, Instruction *> *loads_to_composites,
    std::unordered_map<Instruction *, Instruction *>
        *loads_for_access_chain_to_composites) {
  if (flattened_stage_vars.variable != nullptr) {
    for (Instruction *stage_var_user : stage_var_users) {
      if (!ReplaceStageVar(
              stage_var, stage_var_user, flattened_stage_vars.variable, indices,
              has_extra_arrayness, extra_array_index, loads_to_composites,
              loads_for_access_chain_to_composites)) {
        return false;
      }
    }
    return true;
  }
  for (uint32_t i = 0; i < flattened_stage_vars.flattened_variables.size();
       ++i) {
    indices.push_back(i);
    std::unordered_map<Instruction *, Instruction *> loads_to_component_values;
    std::unordered_map<Instruction *, Instruction *>
        loads_for_access_chain_to_component_values;
    if (!ReplaceStageVars(stage_var, stage_var_users,
                          flattened_stage_vars.flattened_variables[i], indices,
                          has_extra_arrayness, extra_array_index,
                          &loads_to_component_values,
                          &loads_for_access_chain_to_component_values)) {
      return false;
    }
    indices.pop_back();

    uint32_t depth_to_component = static_cast<uint32_t>(indices.size());
    AddComponentsToCompositesForLoads(
        loads_for_access_chain_to_component_values,
        loads_for_access_chain_to_composites, depth_to_component);
    if (has_extra_arrayness)
      ++depth_to_component;
    AddComponentsToCompositesForLoads(loads_to_component_values,
                                      loads_to_composites, depth_to_component);
  }
  return true;
}

bool FlattenArrayMatrixStageVariable::ReplaceStageVar(
    Instruction *stage_var, Instruction *stage_var_user,
    Instruction *flattened_var, const std::vector<uint32_t> &indices,
    bool has_extra_arrayness, uint32_t extra_array_index,
    std::unordered_map<Instruction *, Instruction *> *loads_to_component_values,
    std::unordered_map<Instruction *, Instruction *>
        *loads_for_access_chain_to_component_values) {
  SpvOp opcode = stage_var_user->opcode();
  if (opcode == SpvOpStore) {
    ReplaceStoreWithFlattenedVar(nullptr, stage_var_user, indices,
                                 flattened_var, has_extra_arrayness,
                                 extra_array_index);
    return true;
  }

  if (opcode == SpvOpLoad) {
    ReplaceLoadWithFlattenedVar(nullptr, stage_var_user, flattened_var,
                                loads_to_component_values, has_extra_arrayness,
                                extra_array_index);
    return true;
  }

  // Copy OpName and annotation instructions only once.
  if (extra_array_index != 0)
    return true;

  if (opcode == SpvOpDecorateId || opcode == SpvOpDecorateString ||
      opcode == SpvOpDecorate) {
    CloneAnnotationForVariable(stage_var_user, flattened_var->result_id());
    return true;
  }

  if (opcode == SpvOpName) {
    std::unique_ptr<Instruction> new_inst(stage_var_user->Clone(context()));
    new_inst->SetInOperand(0, {flattened_var->result_id()});
    context()->get_def_use_mgr()->AnalyzeInstDefUse(new_inst.get());
    context()->AddDebug2Inst(std::move(new_inst));
    return true;
  }

  if (opcode == SpvOpEntryPoint) {
    return ReplaceStageVarInEntryPoint(stage_var, stage_var_user,
                                       flattened_var->result_id());
  }

  if (opcode == SpvOpAccessChain) {
    ReplaceAccessChainWithFlattenedVar(
        stage_var_user, indices, flattened_var,
        loads_for_access_chain_to_component_values);
    return true;
  }

  std::string message("Unhandled instruction");
  message += "\n  " + stage_var_user->PrettyPrint(
                          SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
  message += "\nfor stage variable flattening\n  " +
             stage_var->PrettyPrint(SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
  context()->consumer()(SPV_MSG_ERROR, "", {0, 0, 0}, message.c_str());
  return false;
}

void FlattenArrayMatrixStageVariable::UseBaseAccessChainForAccessChain(
    Instruction *access_chain, Instruction *base_access_chain) {
  assert(base_access_chain->opcode() == SpvOpAccessChain &&
         access_chain->opcode() == SpvOpAccessChain &&
         access_chain->GetSingleWordInOperand(0) ==
             base_access_chain->result_id());
  Instruction::OperandList new_operands;
  for (uint32_t i = 0; i < base_access_chain->NumInOperands(); ++i) {
    new_operands.emplace_back(base_access_chain->GetInOperand(i));
  }
  for (uint32_t i = 1; i < access_chain->NumInOperands(); ++i) {
    new_operands.emplace_back(access_chain->GetInOperand(i));
  }
  access_chain->SetInOperands(std::move(new_operands));
}

Instruction *FlattenArrayMatrixStageVariable::CreateAccessChainToVar(
    uint32_t var_type_id, Instruction *var, Instruction *access_chain,
    uint32_t *component_type_id) {
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  *component_type_id = FindComponentTypeOfArrayMatrix(
      def_use_mgr, var_type_id, access_chain->NumInOperands() - 1);

  uint32_t ptr_type_id =
      GetPointerType(*component_type_id,
                     static_cast<SpvStorageClass>(var->GetSingleWordInOperand(
                         kOpVariableStorageClassInOperandIndex)));

  std::unique_ptr<Instruction> new_access_chain(
      new Instruction(context(), SpvOpAccessChain, ptr_type_id, TakeNextId(),
                      std::initializer_list<Operand>{
                          {SPV_OPERAND_TYPE_ID, {var->result_id()}}}));
  for (uint32_t i = 1; i < access_chain->NumInOperands(); ++i) {
    auto operand = access_chain->GetInOperand(i);
    new_access_chain->AddOperand(std::move(operand));
  }

  Instruction *inst = new_access_chain.get();
  def_use_mgr->AnalyzeInstDefUse(inst);
  access_chain->InsertBefore(std::move(new_access_chain));
  return inst;
}

Instruction *FlattenArrayMatrixStageVariable::CreateAccessChainWithIndex(
    uint32_t component_type_id, Instruction *var, uint32_t index,
    Instruction *insert_before) {
  uint32_t ptr_type_id =
      GetPointerType(component_type_id,
                     static_cast<SpvStorageClass>(var->GetSingleWordInOperand(
                         kOpVariableStorageClassInOperandIndex)));
  uint32_t index_id = context()->get_constant_mgr()->GetUIntConst(index);
  std::unique_ptr<Instruction> new_access_chain(
      new Instruction(context(), SpvOpAccessChain, ptr_type_id, TakeNextId(),
                      std::initializer_list<Operand>{
                          {SPV_OPERAND_TYPE_ID, {var->result_id()}},
                          {SPV_OPERAND_TYPE_ID, {index_id}},
                      }));
  Instruction *inst = new_access_chain.get();
  context()->get_def_use_mgr()->AnalyzeInstDefUse(inst);
  insert_before->InsertBefore(std::move(new_access_chain));
  return inst;
}

void FlattenArrayMatrixStageVariable::ReplaceAccessChainWithFlattenedVar(
    Instruction *access_chain, const std::vector<uint32_t> &indices,
    Instruction *flattened_var,
    std::unordered_map<Instruction *, Instruction *>
        *loads_to_component_values) {
  // Note that we have a strong assumption that |access_chain| has only a single
  // index that is for the extra arrayness.
  std::vector<Instruction *> users =
      GetUsersIf(access_chain, [](Instruction *) { return true; });
  for (Instruction *user : users) {
    switch (user->opcode()) {
    case SpvOpAccessChain:
      UseBaseAccessChainForAccessChain(user, access_chain);
      ReplaceAccessChainWithFlattenedVar(user, indices, flattened_var,
                                         loads_to_component_values);
      break;
    case SpvOpStore:
      ReplaceStoreWithFlattenedVar(access_chain, user, indices, flattened_var,
                                   false, 0);
      break;
    case SpvOpLoad:
      ReplaceLoadWithFlattenedVar(access_chain, user, flattened_var,
                                  loads_to_component_values, false, 0);
      break;
    default:
      break;
    }
  }
}

void FlattenArrayMatrixStageVariable::CloneAnnotationForVariable(
    Instruction *annotation_inst, uint32_t var_id) {
  assert(annotation_inst->opcode() == SpvOpDecorate ||
         annotation_inst->opcode() == SpvOpDecorateId ||
         annotation_inst->opcode() == SpvOpDecorateString);
  std::unique_ptr<Instruction> new_inst(annotation_inst->Clone(context()));
  new_inst->SetInOperand(0, {var_id});
  context()->AddAnnotationInst(std::move(new_inst));
}

bool FlattenArrayMatrixStageVariable::ReplaceStageVarInEntryPoint(
    Instruction *stage_var, Instruction *entry_point,
    uint32_t flattened_var_id) {
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  uint32_t stage_var_id = stage_var->result_id();
  if (stage_vars_removed_from_entry_point_operands_.find(stage_var_id) !=
      stage_vars_removed_from_entry_point_operands_.end()) {
    entry_point->AddOperand({SPV_OPERAND_TYPE_ID, {flattened_var_id}});
    def_use_mgr->AnalyzeInstUse(entry_point);
    return true;
  }

  bool success = !entry_point->WhileEachInId(
      [&stage_var_id, &flattened_var_id](uint32_t *id) {
        if (*id == stage_var_id) {
          *id = flattened_var_id;
          return false;
        }
        return true;
      });
  if (!success) {
    std::string message("Stage variable is not an operand of the entry point");
    message += "\n  " +
               stage_var->PrettyPrint(SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
    message += "\n  " + entry_point->PrettyPrint(
                            SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
    context()->consumer()(SPV_MSG_ERROR, "", {0, 0, 0}, message.c_str());
    return false;
  }

  def_use_mgr->AnalyzeInstUse(entry_point);
  stage_vars_removed_from_entry_point_operands_.insert(stage_var_id);
  return true;
}

uint32_t
FlattenArrayMatrixStageVariable::GetPointeeTypeIdOfVar(Instruction *var) {
  assert(var->opcode() == SpvOpVariable);

  uint32_t ptr_type_id = var->type_id();
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  Instruction *ptr_type_inst = def_use_mgr->GetDef(ptr_type_id);

  assert(ptr_type_inst->opcode() == SpvOpTypePointer &&
         "Variable must have a pointer type.");
  return ptr_type_inst->GetSingleWordInOperand(kOpTypePtrTypeInOperandIndex);
}

void FlattenArrayMatrixStageVariable::ReplaceLoadWithFlattenedVar(
    Instruction *access_chain, Instruction *load, Instruction *flattened_var,
    std::unordered_map<Instruction *, Instruction *> *loads_to_component_values,
    bool has_extra_arrayness, uint32_t extra_array_index) {
  uint32_t component_type_id = GetPointeeTypeIdOfVar(flattened_var);
  Instruction *ptr = flattened_var;
  if (access_chain != nullptr) {
    ptr = CreateAccessChainToVar(component_type_id, flattened_var, access_chain,
                                 &component_type_id);
  } else if (has_extra_arrayness) {
    auto *ty_mgr = context()->get_type_mgr();
    analysis::Array *array_type = ty_mgr->GetType(component_type_id)->AsArray();
    assert(array_type != nullptr);
    component_type_id = ty_mgr->GetTypeInstruction(array_type->element_type());
    ptr = CreateAccessChainWithIndex(component_type_id, flattened_var,
                                     extra_array_index, load);
  }
  std::unique_ptr<Instruction> component_value(
      new Instruction(context(), SpvOpLoad, component_type_id, TakeNextId(),
                      std::initializer_list<Operand>{
                          {SPV_OPERAND_TYPE_ID, {ptr->result_id()}}}));
  loads_to_component_values->insert({load, component_value.get()});
  context()->get_def_use_mgr()->AnalyzeInstDefUse(component_value.get());
  load->InsertBefore(std::move(component_value));
}

void FlattenArrayMatrixStageVariable::ReplaceStoreWithFlattenedVar(
    Instruction *access_chain, Instruction *store,
    const std::vector<uint32_t> &indices, Instruction *flattened_var,
    bool has_extra_arrayness, uint32_t extra_array_index) {
  uint32_t component_type_id = GetPointeeTypeIdOfVar(flattened_var);
  Instruction *ptr = flattened_var;
  if (access_chain != nullptr) {
    ptr = CreateAccessChainToVar(component_type_id, flattened_var, access_chain,
                                 &component_type_id);
  } else if (has_extra_arrayness) {
    // If it has an extra arrayness
    auto *ty_mgr = context()->get_type_mgr();
    analysis::Array *array_type = ty_mgr->GetType(component_type_id)->AsArray();
    assert(array_type != nullptr);
    component_type_id = ty_mgr->GetTypeInstruction(array_type->element_type());
    ptr = CreateAccessChainWithIndex(component_type_id, flattened_var,
                                     extra_array_index, store);
  }

  uint32_t value_id = store->GetSingleWordInOperand(1);
  uint32_t component_id = TakeNextId();
  std::unique_ptr<Instruction> composite_extract(new Instruction(
      context(), SpvOpCompositeExtract, component_type_id, component_id,
      std::initializer_list<Operand>{{SPV_OPERAND_TYPE_ID, {value_id}}}));
  if (has_extra_arrayness) {
    composite_extract->AddOperand(
        {SPV_OPERAND_TYPE_LITERAL_INTEGER, {extra_array_index}});
  }
  for (uint32_t index : indices) {
    composite_extract->AddOperand({SPV_OPERAND_TYPE_LITERAL_INTEGER, {index}});
  }

  std::unique_ptr<Instruction> new_store(store->Clone(context()));
  new_store->SetInOperand(0, {ptr->result_id()});
  new_store->SetInOperand(1, {component_id});

  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  def_use_mgr->AnalyzeInstDefUse(composite_extract.get());
  def_use_mgr->AnalyzeInstDefUse(new_store.get());

  store->InsertBefore(std::move(composite_extract));
  store->InsertBefore(std::move(new_store));
}

Instruction *
FlattenArrayMatrixStageVariable::CreateCompositeConstructForComponentOfLoad(
    Instruction *load, uint32_t depth_to_component) {
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  uint32_t type_id = load->type_id();
  if (depth_to_component != 0) {
    type_id = FindComponentTypeOfArrayMatrix(def_use_mgr, load->type_id(),
                                             depth_to_component);
  }
  uint32_t new_id = context()->TakeNextId();
  std::unique_ptr<Instruction> new_composite_construct(
      new Instruction(context(), SpvOpCompositeConstruct, type_id, new_id, {}));
  Instruction *composite_construct = new_composite_construct.get();
  def_use_mgr->AnalyzeInstDefUse(composite_construct);

  // Insert |new_composite_construct| after |load|. When there are multiple
  // recursive composite construct instructions for a load, we have to place the
  // composite construct with a lower depth later because it constructs the
  // composite that contains other composites with lower depths.
  auto *insert_before = load->NextNode();
  while (true) {
    auto itr =
        composite_ids_to_component_depths.find(insert_before->result_id());
    if (itr == composite_ids_to_component_depths.end())
      break;
    if (itr->second <= depth_to_component)
      break;
    insert_before = insert_before->NextNode();
  }
  insert_before->InsertBefore(std::move(new_composite_construct));
  composite_ids_to_component_depths.insert({new_id, depth_to_component});
  return composite_construct;
}

void FlattenArrayMatrixStageVariable::AddComponentsToCompositesForLoads(
    const std::unordered_map<Instruction *, Instruction *>
        &loads_to_component_values,
    std::unordered_map<Instruction *, Instruction *> *loads_to_composites,
    uint32_t depth_to_component) {
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  for (auto &load_and_component_vale : loads_to_component_values) {
    Instruction *load = load_and_component_vale.first;
    Instruction *component_value = load_and_component_vale.second;
    Instruction *composite_construct = nullptr;
    auto itr = loads_to_composites->find(load);
    if (itr == loads_to_composites->end()) {
      composite_construct =
          CreateCompositeConstructForComponentOfLoad(load, depth_to_component);
      loads_to_composites->insert({load, composite_construct});
    } else {
      composite_construct = itr->second;
    }
    composite_construct->AddOperand(
        {SPV_OPERAND_TYPE_ID, {component_value->result_id()}});
    def_use_mgr->AnalyzeInstDefUse(composite_construct);
  }
}

std::vector<Instruction *> FlattenArrayMatrixStageVariable::GetUsersIf(
    Instruction *ptr, const std::function<bool(Instruction *)> &condition) {
  std::vector<Instruction *> users;
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  def_use_mgr->ForEachUser(ptr, [&users, &condition](Instruction *user) {
    if (condition(user))
      users.push_back(user);
  });
  return users;
}

uint32_t FlattenArrayMatrixStageVariable::GetArrayType(uint32_t elem_type_id,
                                                       uint32_t array_length) {
  analysis::Type *elem_type = context()->get_type_mgr()->GetType(elem_type_id);
  uint32_t array_length_id =
      context()->get_constant_mgr()->GetUIntConst(array_length);
  analysis::Array array_type(
      elem_type,
      analysis::Array::LengthInfo{array_length_id, {0, array_length}});
  return context()->get_type_mgr()->GetTypeInstruction(&array_type);
}

uint32_t
FlattenArrayMatrixStageVariable::GetPointerType(uint32_t type_id,
                                                SpvStorageClass storage_class) {
  analysis::Type *type = context()->get_type_mgr()->GetType(type_id);
  analysis::Pointer ptr_type(type, storage_class);
  return context()->get_type_mgr()->GetTypeInstruction(&ptr_type);
}

FlattenedVariables
FlattenArrayMatrixStageVariable::CreateFlattenedStageVarsForArray(
    Instruction *stage_var_type, SpvStorageClass storage_class,
    uint32_t extra_array_length) {
  assert(stage_var_type->opcode() == SpvOpTypeArray);

  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  uint32_t array_length = GetArrayLength(def_use_mgr, stage_var_type);
  Instruction *elem_type = GetArrayElementType(def_use_mgr, stage_var_type);

  FlattenedVariables flattened_vars;
  while (array_length > 0) {
    FlattenedVariables flattened_vars_for_element =
        CreateFlattenedStageVarsForReplacement(elem_type, storage_class,
                                               extra_array_length);
    flattened_vars.flattened_variables.push_back(flattened_vars_for_element);
    --array_length;
  }
  return flattened_vars;
}

FlattenedVariables
FlattenArrayMatrixStageVariable::CreateFlattenedStageVarsForMatrix(
    Instruction *stage_var_type, SpvStorageClass storage_class,
    uint32_t extra_array_length) {
  assert(stage_var_type->opcode() == SpvOpTypeMatrix);

  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  uint32_t column_count = stage_var_type->GetSingleWordInOperand(
      kOpTypeMatrixColCountInOperandIndex);
  Instruction *column_type = GetMatrixColumnType(def_use_mgr, stage_var_type);

  FlattenedVariables flattened_vars;
  while (column_count > 0) {
    FlattenedVariables flattened_vars_for_column =
        CreateFlattenedStageVarsForReplacement(column_type, storage_class,
                                               extra_array_length);
    flattened_vars.flattened_variables.push_back(flattened_vars_for_column);
    --column_count;
  }
  return flattened_vars;
}

FlattenedVariables
FlattenArrayMatrixStageVariable::CreateFlattenedStageVarsForReplacement(
    Instruction *stage_var_type, SpvStorageClass storage_class,
    uint32_t extra_array_length) {
  // Handle array case.
  if (stage_var_type->opcode() == SpvOpTypeArray) {
    return CreateFlattenedStageVarsForArray(stage_var_type, storage_class,
                                            extra_array_length);
  }

  // Handle matrix case.
  if (stage_var_type->opcode() == SpvOpTypeMatrix) {
    return CreateFlattenedStageVarsForMatrix(stage_var_type, storage_class,
                                             extra_array_length);
  }

  // Handle scalar or vector case.
  FlattenedVariables flattened_var;
  uint32_t type_id = stage_var_type->result_id();
  if (extra_array_length != 0) {
    type_id = GetArrayType(type_id, extra_array_length);
  }
  uint32_t ptr_type_id =
      context()->get_type_mgr()->FindPointerToType(type_id, storage_class);
  uint32_t id = TakeNextId();
  std::unique_ptr<Instruction> variable(
      new Instruction(context(), SpvOpVariable, ptr_type_id, id,
                      std::initializer_list<Operand>{
                          {SPV_OPERAND_TYPE_STORAGE_CLASS,
                           {static_cast<uint32_t>(storage_class)}}}));
  flattened_var.variable = variable.get();
  context()->AddGlobalValue(std::move(variable));
  return flattened_var;
}

Instruction *FlattenArrayMatrixStageVariable::GetTypeOfArrayOrMatrixStageVar(
    Instruction *stage_var, bool has_extra_arrayness) {
  uint32_t pointee_type_id = GetPointeeTypeIdOfVar(stage_var);

  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  Instruction *type_inst = def_use_mgr->GetDef(pointee_type_id);
  if (has_extra_arrayness) {
    assert(type_inst->opcode() == SpvOpTypeArray &&
           "Stage variable with an extra arrayness must be an array");
    // Get the type without extra arrayness.
    uint32_t elem_type_id =
        type_inst->GetSingleWordInOperand(kOpTypeArrayElemTypeInOperandIndex);
    type_inst = def_use_mgr->GetDef(elem_type_id);
  }

  if (type_inst->opcode() != SpvOpTypeArray &&
      type_inst->opcode() != SpvOpTypeMatrix) {
    return nullptr;
  }
  return type_inst;
}

Instruction *
FlattenArrayMatrixStageVariable::GetStageVariable(uint32_t stage_var_id) {
  analysis::DefUseManager *def_use_mgr = context()->get_def_use_mgr();
  Instruction *stage_var = def_use_mgr->GetDef(stage_var_id);
  if (stage_var == nullptr) {
    std::string message("Stage variable does not exist");
    context()->consumer()(SPV_MSG_ERROR, "", {0, 0, 0}, message.c_str());
    return nullptr;
  }
  if (stage_var->opcode() != SpvOpVariable) {
    std::string message("Stage variable must be OpVariable instruction");
    message += "\n  " +
               stage_var->PrettyPrint(SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
    context()->consumer()(SPV_MSG_ERROR, "", {0, 0, 0}, message.c_str());
    return nullptr;
  }
  return stage_var;
}

Pass::Status FlattenArrayMatrixStageVariable::Process() {
  std::unordered_map<uint32_t, StageVariableLocationInfo>
      stage_var_ids_to_stage_var_location_info;
  CollectStageVariablesToFlatten(&stage_var_ids_to_stage_var_location_info);

  Pass::Status status = Status::SuccessWithoutChange;
  for (auto itr : stage_var_ids_to_stage_var_location_info) {
    Instruction *stage_var = GetStageVariable(itr.first);
    if (stage_var == nullptr)
      return Pass::Status::Failure;

    Instruction *stage_var_type = GetTypeOfArrayOrMatrixStageVar(
        stage_var, itr.second.extra_arrayness != 0);
    if (stage_var_type == nullptr)
      continue;

    if (!FlattenStageVariable(stage_var, stage_var_type, itr.second)) {
      return Pass::Status::Failure;
    }
    status = Pass::Status::SuccessWithChange;
  }

  return status;
}

} // namespace opt
} // namespace spvtools
