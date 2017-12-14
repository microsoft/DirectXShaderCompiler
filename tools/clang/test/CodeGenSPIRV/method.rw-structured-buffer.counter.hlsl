// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

// CHECK: OpMemberDecorate %type_ACSBuffer_counter 0 Offset 0
// CHECK: OpDecorate %type_ACSBuffer_counter BufferBlock

// CHECK: %type_ACSBuffer_counter = OpTypeStruct %int

// CHECK: %counter_var_wCounter1 = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform
// CHECK: %counter_var_wCounter2 = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform
RWStructuredBuffer<S> wCounter1;
RWStructuredBuffer<S> wCounter2;
// CHECK: %counter_var_woCounter = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform
RWStructuredBuffer<S> woCounter;

float4 main() : SV_Target {
// CHECK:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_wCounter1 %uint_0
// CHECK-NEXT: [[pre1:%\d+]] = OpAtomicIAdd %int [[ptr1]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[val1:%\d+]] = OpBitcast %uint [[pre1]]
// CHECK-NEXT:                 OpStore %a [[val1]]
    uint a = wCounter1.IncrementCounter();
// CHECK:      [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_wCounter2 %uint_0
// CHECK-NEXT: [[pre2:%\d+]] = OpAtomicISub %int [[ptr2]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[cnt2:%\d+]] = OpISub %int [[pre2]] %int_1
// CHECK-NEXT: [[val2:%\d+]] = OpBitcast %uint [[cnt2]]
// CHECK-NEXT:                 OpStore %b [[val2]]
    uint b = wCounter2.DecrementCounter();

    return woCounter[0].f + float4(a, b, 0, 0);
}
