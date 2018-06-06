// Run: %dxc -T vs_6_0 -E main

struct S {
    float    a;
    float3   b;
    float2x3 c;
};

AppendStructuredBuffer<float4> buffer1;
AppendStructuredBuffer<S>      buffer2;

// Use this to test appending a consumed value.
ConsumeStructuredBuffer<float4> buffer3;

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

// CHECK-NEXT: [[s_a:%\d+]] = OpCompositeExtract %float [[s]] 0
// CHECK-NEXT: [[s_b:%\d+]] = OpCompositeExtract %v3float [[s]] 1
// CHECK-NEXT: [[s_c:%\d+]] = OpCompositeExtract %mat2v3float [[s]] 2

// CHECK-NEXT: [[val:%\d+]] = OpCompositeConstruct %S [[s_a]] [[s_b]] [[s_c]]
// CHECK-NEXT: OpStore [[buffer2]] [[val]]
    buffer2.Append(s);

// CHECK:           [[buffer1:%\d+]] = OpAccessChain %_ptr_Uniform_v4float %buffer1 %uint_0 {{%\d+}}
// CHECK:      [[consumed_ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v4float %buffer3 %uint_0 {{%\d+}}
// CHECK-NEXT:     [[consumed:%\d+]] = OpLoad %v4float [[consumed_ptr]]
// CHECK-NEXT:                         OpStore [[buffer1]] [[consumed]]
    buffer1.Append(buffer3.Consume());
}
