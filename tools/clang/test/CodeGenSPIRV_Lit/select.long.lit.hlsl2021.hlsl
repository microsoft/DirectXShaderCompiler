// RUN: %dxc -T ps_6_0 -HV 2021 -E main

// Check that the literals get a 64-bit type, and the result of the select is
// then cast to an unsigned 64-bit value.
void foo(uint x) {
// CHECK:      %foo = OpFunction
// CHECK-NEXT: [[param:%\w+]] = OpFunctionParameter %_ptr_Function_uint
// CHECK-NEXT: OpLabel
// CHECK-NEXT: [[value:%\w+]] = OpVariable %_ptr_Function_ulong Function
// CHECK-NEXT: [[temp:%\w+]] = OpVariable %_ptr_Function_long Function
// CHECK-NEXT: [[ld:%\w+]] = OpLoad %uint [[param]]
// CHECK-NEXT: [[cmp:%\w+]] = OpULessThan %bool [[ld]] %uint_64
// CHECK-NEXT: OpSelectionMerge [[merge_bb:%\w+]] None
// CHECK-NEXT: OpBranchConditional [[cmp]] [[true_bb:%\w+]] [[false_bb:%\w+]]
// CHECK-NEXT: [[true_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %long_1
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[false_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %long_0
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[merge_bb]] = OpLabel
// CHECK-NEXT: [[ld2:%\w+]] = OpLoad %long [[temp]]
// CHECK-NEXT: [[res:%\w+]] = OpBitcast %ulong [[ld2]]
// CHECK-NEXT: OpStore [[value]] [[res:%\w+]]
  uint64_t value = x < 64 ? 1 : 0;
}

// Check that the literals get a 64-bit type, and the result of the select is
// then cast to an signed 64-bit value. Note that the bitcast is redundant in
// this case, but we add the bitcast before the type of the literal has been
// determined, so we add it anyway.
void bar(uint x) {
// CHECK:      %bar = OpFunction
// CHECK-NEXT: [[param:%\w+]] = OpFunctionParameter %_ptr_Function_uint
// CHECK-NEXT: OpLabel
// CHECK-NEXT: [[value:%\w+]] = OpVariable %_ptr_Function_long Function
// CHECK-NEXT: [[temp:%\w+]] = OpVariable %_ptr_Function_long Function
// CHECK-NEXT: [[ld:%\w+]] = OpLoad %uint [[param]]
// CHECK-NEXT: [[cmp:%\w+]] = OpULessThan %bool [[ld]] %uint_64
// CHECK-NEXT: OpSelectionMerge [[merge_bb:%\w+]] None
// CHECK-NEXT: OpBranchConditional [[cmp]] [[true_bb:%\w+]] [[false_bb:%\w+]]
// CHECK-NEXT: [[true_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %long_1
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[false_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %long_0
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[merge_bb]] = OpLabel
// CHECK-NEXT: [[ld2:%\w+]] = OpLoad %long [[temp]]
// CHECK-NEXT: [[res:%\w+]] = OpBitcast %long [[ld2]]
// CHECK-NEXT: OpStore [[value]] [[res:%\w+]]
  int64_t value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(2);
  bar(2);
}
