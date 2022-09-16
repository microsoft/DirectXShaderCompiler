// RUN: %dxc -T ps_6_0 -E main -HV 2021

void main() {
  // CHECK-LABEL: %bb_entry = OpLabel

  bool a, b, c;
  // CHECK:      [[t:%temp_[a-z_]+]] = OpVariable %_ptr_Function_bool Function
  // CHECK-NEXT: [[a:%\d+]] = OpLoad %bool %a
  // CHECK-NEXT: OpStore [[t]] %false
  // CHECK-NEXT: OpSelectionMerge %logical_merge None
  // CHECK-NEXT: OpBranchConditional [[a]] %logical_lhs_cond %logical_merge
  // CHECK-NEXT: %logical_lhs_cond = OpLabel
  // CHECK-NEXT: [[b:%\d+]] = OpLoad %bool %b
  // CHECK-NEXT: OpStore [[t]] [[b]]
  // CHECK-NEXT: OpBranch %logical_merge
  // CHECK-NEXT: %logical_merge = OpLabel
  // CHECK-NEXT: [[result:%\d+]] = OpLoad %bool [[t]]
  // CHECK-NEXT: OpStore %c [[result]]
  c = a && b;
}
