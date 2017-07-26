// Run: %dxc -T ps_6_0 -E main

// Note: we need to consider the order of basic blocks. So CHECK-NEXT is used
// extensively.

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    bool c;
    int val;

    // Both then and else
// CHECK:      [[c0:%\d+]] = OpLoad %bool %c
// CHECK-NEXT: OpSelectionMerge %if_merge None
// CHECK-NEXT: OpBranchConditional [[c0]] %if_true %if_false
    if (c) {
// CHECK-LABEL: %if_true = OpLabel
// CHECK-NEXT: [[val0:%\d+]] = OpLoad %int %val
// CHECK-NEXT: [[val1:%\d+]] = OpIAdd %int [[val0]] %int_1
// CHECK-NEXT: OpStore %val [[val1]]
// CHECK-NEXT: OpBranch %if_merge
        val = val + 1;
    } else {
// CHECK-LABEL: %if_false = OpLabel
// CHECK-NEXT: [[val2:%\d+]] = OpLoad %int %val
// CHECK-NEXT: [[val3:%\d+]] = OpIAdd %int [[val2]] %int_2
// CHECK-NEXT: OpStore %val [[val3]]
// CHECK-NEXT: OpBranch %if_merge
        val = val + 2;
    }
// CHECK-LABEL: %if_merge = OpLabel

    // No else
// CHECK-NEXT: [[c1:%\d+]] = OpLoad %bool %c
// CHECK-NEXT: OpSelectionMerge %if_merge_0 None
// CHECK-NEXT: OpBranchConditional [[c1]] %if_true_0 %if_merge_0
    if (c)
// CHECK-LABEL: %if_true_0 = OpLabel
// CHECK-NEXT: OpStore %val %int_1
// CHECK-NEXT: OpBranch %if_merge_0
        val = 1;
// CHECK-LABEL: %if_merge_0 = OpLabel

    // Empty then
// CHECK-NEXT: [[c2:%\d+]] = OpLoad %bool %c
// CHECK-NEXT: OpSelectionMerge %if_merge_1 None
// CHECK-NEXT: OpBranchConditional [[c2]] %if_true_1 %if_false_0
    if (c) {
// CHECK-LABEL: %if_true_1 = OpLabel
// CHECK-NEXT: OpBranch %if_merge_1
    } else {
// CHECK-LABEL: %if_false_0 = OpLabel
// CHECK-NEXT: OpStore %val %int_2
// CHECK-NEXT: OpBranch %if_merge_1
        val = 2;
    }
// CHECK-LABEL: %if_merge_1 = OpLabel

    // Null body
// CHECK-NEXT: [[c3:%\d+]] = OpLoad %bool %c
// CHECK-NEXT: OpSelectionMerge %if_merge_2 None
// CHECK-NEXT: OpBranchConditional [[c3]] %if_true_2 %if_merge_2
    if (c)
// CHECK-LABEL: %if_true_2 = OpLabel
// CHECK-NEXT: OpBranch %if_merge_2
        ;

// CHECK-LABEL: %if_merge_2 = OpLabel
// CHECK-NEXT: OpReturn
}
