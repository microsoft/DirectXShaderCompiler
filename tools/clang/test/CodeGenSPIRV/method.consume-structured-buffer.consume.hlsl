// Run: %dxc -T vs_6_0 -E main

struct S {
    float    a;
    float3   b;
    float2x3 c;
};

struct T {
    S        s[5];
};

ConsumeStructuredBuffer<float4> buffer1;
ConsumeStructuredBuffer<S>      buffer2;
ConsumeStructuredBuffer<T>      buffer3;

float4 main() : A {
// CHECK:      [[counter:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_buffer1 %uint_0
// CHECK-NEXT: [[prev:%\d+]] = OpAtomicISub %int [[counter]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[index:%\d+]] = OpISub %int [[prev]] %int_1
// CHECK-NEXT: [[buffer1:%\d+]] = OpAccessChain %_ptr_Uniform_v4float %buffer1 %uint_0 [[index]]
// CHECK-NEXT: [[val:%\d+]] = OpLoad %v4float [[buffer1]]
// CHECK-NEXT: OpStore %v [[val]]
    float4 v = buffer1.Consume();

    S s; // Will use a separate S type without layout decorations

// CHECK-NEXT: [[counter:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_buffer2 %uint_0
// CHECK-NEXT: [[prev:%\d+]] = OpAtomicISub %int [[counter]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[index:%\d+]] = OpISub %int [[prev]] %int_1

// CHECK-NEXT: [[buffer2:%\d+]] = OpAccessChain %_ptr_Uniform_S %buffer2 %uint_0 [[index]]
// CHECK-NEXT: [[val:%\d+]] = OpLoad %S [[buffer2]]

// CHECK-NEXT: [[buffer20:%\d+]] = OpCompositeExtract %float [[val]] 0
// CHECK-NEXT: [[s0:%\d+]] = OpAccessChain %_ptr_Function_float %s %uint_0
// CHECK-NEXT: OpStore [[s0]] [[buffer20]]

// CHECK-NEXT: [[buffer21:%\d+]] = OpCompositeExtract %v3float [[val]] 1
// CHECK-NEXT: [[s1:%\d+]] = OpAccessChain %_ptr_Function_v3float %s %uint_1
// CHECK-NEXT: OpStore [[s1]] [[buffer21]]

// CHECK-NEXT: [[buffer22:%\d+]] = OpCompositeExtract %mat2v3float [[val]] 2
// CHECK-NEXT: [[s2:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %s %uint_2
// CHECK-NEXT: OpStore [[s2]] [[buffer22]]
    s = buffer2.Consume();

// CHECK:      [[counter:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_buffer3 %uint_0
// CHECK-NEXT: [[prev:%\d+]] = OpAtomicISub %int [[counter]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[index:%\d+]] = OpISub %int [[prev]] %int_1
// CHECK-NEXT: [[buffer3:%\d+]] = OpAccessChain %_ptr_Uniform_T %buffer3 %uint_0 [[index]]
// CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_Uniform_v3float [[buffer3]] %int_0 %int_3 %int_1
// CHECK-NEXT: [[val:%\d+]] = OpLoad %v3float [[ac]]
// CHECK-NEXT: OpStore %val [[val]]
    float3 val = buffer3.Consume().s[3].b;

    return v;
}
