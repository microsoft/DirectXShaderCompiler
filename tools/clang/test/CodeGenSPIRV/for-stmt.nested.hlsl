// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
// CHECK-NEXT: %val = OpVariable %_ptr_Function_int Function %int_0
    int val = 0;

// CHECK-NEXT: %i = OpVariable %_ptr_Function_int Function %int_0
// CHECK-NEXT: %j = OpVariable %_ptr_Function_int Function %int_0
// CHECK-NEXT: %k = OpVariable %_ptr_Function_int Function %int_0
// CHECK-NEXT: OpBranch %for_check

// CHECK-LABEL: %for_check = OpLabel
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %int %i
// CHECK-NEXT: [[lt0:%\d+]] = OpSLessThan %bool [[i0]] %int_10
// CHECK-NEXT: OpLoopMerge %for_merge %for_continue None
// CHECK-NEXT: OpBranchConditional [[lt0]] %for_body %for_merge
    for (int i = 0; i < 10; ++i) {
// CHECK-LABEL: %for_body = OpLabel
// CHECK-NEXT: [[val0:%\d+]] = OpLoad %int %val
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %int %i
// CHECK-NEXT: [[add0:%\d+]] = OpIAdd %int [[val0]] [[i1]]
// CHECK-NEXT: OpStore %val [[add0]]
        val = val + i;
// CHECK-NEXT: OpBranch %for_check_0

// CHECK-LABEL: %for_check_0 = OpLabel
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %int %j
// CHECK-NEXT: [[lt1:%\d+]] = OpSLessThan %bool [[j0]] %int_10
// CHECK-NEXT: OpLoopMerge %for_merge_0 %for_continue_0 None
// CHECK-NEXT: OpBranchConditional [[lt1]] %for_body_0 %for_merge_0
        for (int j = 0; j < 10; ++j) {
// CHECK-LABEL: %for_body_0 = OpLabel
// CHECK-NEXT: OpBranch %for_check_1

// CHECK-LABEL: %for_check_1 = OpLabel
// CHECK-NEXT: [[k0:%\d+]] = OpLoad %int %k
// CHECK-NEXT: [[lt2:%\d+]] = OpSLessThan %bool [[k0]] %int_10
// CHECK-NEXT: OpLoopMerge %for_merge_1 %for_continue_1 None
// CHECK-NEXT: OpBranchConditional [[lt2]] %for_body_1 %for_merge_1
            for (int k = 0; k < 10; ++k) {
// CHECK-LABEL: %for_body_1 = OpLabel
// CHECK-NEXT: [[val1:%\d+]] = OpLoad %int %val
// CHECK-NEXT: [[k1:%\d+]] = OpLoad %int %k
// CHECK-NEXT: [[add1:%\d+]] = OpIAdd %int [[val1]] [[k1]]
// CHECK-NEXT: OpStore %val [[add1]]
// CHECK-NEXT: OpBranch %for_continue_1
                val = val + k;

// CHECK-LABEL: %for_continue_1 = OpLabel
// CHECK-NEXT: [[k2:%\d+]] = OpLoad %int %k
// CHECK-NEXT: [[add2:%\d+]] = OpIAdd %int [[k2]] %int_1
// CHECK-NEXT: OpStore %k [[add2]]
// CHECK-NEXT: OpBranch %for_check_1
            }

// CHECK-LABEL: %for_merge_1 = OpLabel
// CHECK-NEXT: [[val2:%\d+]] = OpLoad %int %val
// CHECK-NEXT: [[mul0:%\d+]] = OpIMul %int [[val2]] %int_2
// CHECK-NEXT: OpStore %val [[mul0]]
// CHECK-NEXT: OpBranch %for_continue_0
            val = val * 2;

// CHECK-LABEL: %for_continue_0 = OpLabel
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %int %j
// CHECK-NEXT: [[add3:%\d+]] = OpIAdd %int [[j1]] %int_1
// CHECK-NEXT: OpStore %j [[add3]]
// CHECK-NEXT: OpBranch %for_check_0
        }
// CHECK-LABEL: %for_merge_0 = OpLabel
// CHECK-NEXT: OpBranch %for_continue

// CHECK-LABEL: %for_continue = OpLabel
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %int %i
// CHECK-NEXT: [[add4:%\d+]] = OpIAdd %int [[i2]] %int_1
// CHECK-NEXT: OpStore %i [[add4]]
// CHECK-NEXT: OpBranch %for_check
    }

// CHECK-LABEL: %for_merge = OpLabel
// CHECK-NEXT: OpReturn
}
