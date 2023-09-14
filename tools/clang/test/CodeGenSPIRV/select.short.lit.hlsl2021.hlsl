// RUN: %dxc -T ps_6_2 -HV 2021 -E main -enable-16bit-types

// Check that the literals get a 16-bit type, and the result of the select is
// then cast to an unsigned 16-bit value.
void foo(uint x) {
// CHECK:      %foo = OpFunction
// CHECK-NEXT: [[param:%\w+]] = OpFunctionParameter %_ptr_Function_uint
// CHECK-NEXT: OpLabel
// CHECK-NEXT: [[value:%\w+]] = OpVariable %_ptr_Function_ushort Function
// CHECK-NEXT: [[temp:%\w+]] = OpVariable %_ptr_Function_short Function
// CHECK-NEXT: [[ld:%\w+]] = OpLoad %uint [[param]]
// CHECK-NEXT: [[cmp:%\w+]] = OpULessThan %bool [[ld]] %uint_64
// CHECK-NEXT: OpSelectionMerge [[merge_bb:%\w+]] None
// CHECK-NEXT: OpBranchConditional [[cmp]] [[true_bb:%\w+]] [[false_bb:%\w+]]
// CHECK-NEXT: [[true_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %short_1
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[false_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %short_0
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[merge_bb]] = OpLabel
// CHECK-NEXT: [[ld2:%\w+]] = OpLoad %short [[temp]]
// CHECK-NEXT: [[res:%\w+]] = OpBitcast %ushort [[ld2]]
// CHECK-NEXT: OpStore [[value]] [[res:%\w+]]
  uint16_t value = x < 64 ? 1 : 0;
}

// Check that the literals get a 16-bit type, and the result of the select is
// then cast to an signed 16-bit value. Note that the bitcast is redundant in
// this case, but we add the bitcast before the type of the literal has been
// determined, so we add it anyway.
void bar(uint x) {
// CHECK:      %bar = OpFunction
// CHECK-NEXT: [[param:%\w+]] = OpFunctionParameter %_ptr_Function_uint
// CHECK-NEXT: OpLabel
// CHECK-NEXT: [[value:%\w+]] = OpVariable %_ptr_Function_short Function
// CHECK-NEXT: [[temp:%\w+]] = OpVariable %_ptr_Function_short Function
// CHECK-NEXT: [[ld:%\w+]] = OpLoad %uint [[param]]
// CHECK-NEXT: [[cmp:%\w+]] = OpULessThan %bool [[ld]] %uint_64
// CHECK-NEXT: OpSelectionMerge [[merge_bb:%\w+]] None
// CHECK-NEXT: OpBranchConditional [[cmp]] [[true_bb:%\w+]] [[false_bb:%\w+]]
// CHECK-NEXT: [[true_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %short_1
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[false_bb]] = OpLabel
// CHECK-NEXT: OpStore [[temp]] %short_0
// CHECK-NEXT: OpBranch [[merge_bb]]
// CHECK-NEXT: [[merge_bb]] = OpLabel
// CHECK-NEXT: [[ld2:%\w+]] = OpLoad %short [[temp]]
// CHECK-NEXT: [[res:%\w+]] = OpBitcast %short [[ld2]]
// CHECK-NEXT: OpStore [[value]] [[res:%\w+]]
  int16_t value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(2);
  bar(2);
}
