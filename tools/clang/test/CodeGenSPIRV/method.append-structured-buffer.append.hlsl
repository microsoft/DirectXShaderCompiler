// Run: %dxc -T vs_6_0 -E main

struct S {
    float    a;
    float3   b;
    float2x3 c;
};

AppendStructuredBuffer<float4> buffer1;
AppendStructuredBuffer<S>      buffer2;

void main(float4 vec: A) {
// CHECK:      [[counter:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_buffer1 %uint_0
// CHECK-NEXT: [[index:%\d+]] = OpAtomicIAdd %int [[counter]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[buffer1:%\d+]] = OpAccessChain %_ptr_Uniform_v4float %buffer1 %uint_0 [[index]]
// CHECK-NEXT: [[vec:%\d+]] = OpLoad %v4float %vec
// CHECK-NEXT: OpStore [[buffer1]] [[vec]]
    buffer1.Append(vec);

    S s; // Will use a separate S type without layout decorations

// CHECK-NEXT: [[counter:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_buffer2 %uint_0
// CHECK-NEXT: [[index:%\d+]] = OpAtomicIAdd %int [[counter]] %uint_1 %uint_0 %int_1

// CHECK-NEXT: [[buffer2:%\d+]] = OpAccessChain %_ptr_Uniform_S %buffer2 %uint_0 [[index]]
// CHECK-NEXT: [[s:%\d+]] = OpLoad %S_0 %s

// CHECK-NEXT: [[s0:%\d+]] = OpCompositeExtract %float [[s]] 0
// CHECK-NEXT: [[buffer20:%\d+]] = OpAccessChain %_ptr_Uniform_float [[buffer2]] %uint_0
// CHECK-NEXT: OpStore [[buffer20]] [[s0]]

// CHECK-NEXT: [[s1:%\d+]] = OpCompositeExtract %v3float [[s]] 1
// CHECK-NEXT: [[buffer21:%\d+]] = OpAccessChain %_ptr_Uniform_v3float [[buffer2]] %uint_1
// CHECK-NEXT: OpStore [[buffer21]] [[s1]]

// CHECK-NEXT: [[s2:%\d+]] = OpCompositeExtract %mat2v3float [[s]] 2
// CHECK-NEXT: [[buffer22:%\d+]] = OpAccessChain %_ptr_Uniform_mat2v3float [[buffer2]] %uint_2
// CHECK-NEXT: OpStore [[buffer22]] [[s2]]
    buffer2.Append(s);
}
