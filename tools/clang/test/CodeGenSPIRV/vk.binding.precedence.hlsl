// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

// Verify that descriptor set-indicating override per: vk::binding > :register
// using variations on these and vk::counter_binding

// explicit set vk::binding > :register
[[vk::binding(5, 4)]]
RWBuffer<float4> exBindVsReg : register(u6, space7);
// CHECK:      OpDecorate %exBindVsReg DescriptorSet 4
// CHECK-NEXT: OpDecorate %exBindVsReg Binding 5

// implicit set vk::binding > :register
[[vk::binding(8)]]
cbuffer impBindVsReg : register(b9, space1) {
    float cbfield;
};
// CHECK:      OpDecorate %impBindVsReg DescriptorSet 0
// CHECK-NEXT: OpDecorate %impBindVsReg Binding 8

// explicit set vk::binding > :register explicit counter
[[vk::binding(4,1), vk::counter_binding(5)]]
RWStructuredBuffer<S> exBindVsRegExCt : register(u4, space9);
// CHECK:      OpDecorate %exBindVsRegExCt DescriptorSet 1
// CHECK-NEXT: OpDecorate %exBindVsRegExCt Binding 4
// CHECK-NEXT: OpDecorate %counter_var_exBindVsRegExCt DescriptorSet 1
// CHECK-NEXT: OpDecorate %counter_var_exBindVsRegExCt Binding 5

// implicit set vk::binding > :register explicit counter
[[vk::binding(2), vk::counter_binding(5)]]
AppendStructuredBuffer<S> impBindVsRegExCt;
// CHECK-NEXT: OpDecorate %impBindVsRegExCt DescriptorSet 0
// CHECK-NEXT: OpDecorate %impBindVsRegExCt Binding 2
// CHECK-NEXT: OpDecorate %counter_var_impBindVsRegExCt DescriptorSet 0
// CHECK-NEXT: OpDecorate %counter_var_impBindVsRegExCt Binding 5

// explicit set vk::binding > :register implicit counter (main part)
[[vk::binding(4, 3)]]
ConsumeStructuredBuffer<S> exBindVsRegImpCt : register(u3, space9);
// CHECK:      OpDecorate %exBindVsRegImpCt DescriptorSet 3
// CHECK-NEXT: OpDecorate %exBindVsRegImpCt Binding 4

// implicit set vk::binding > :register implicit counter (main part)
[[vk::binding(12)]]
RWStructuredBuffer<S> impBindVsRegImpCt : register(u9, space4);
// CHECK:      OpDecorate %impBindVsRegImpCt DescriptorSet 0
// CHECK-NEXT: OpDecorate %impBindVsRegImpCt Binding 12

// explicit set vk::binding > :register implicit counter (counter part)
// CHECK:      OpDecorate %counter_var_exBindVsRegImpCt DescriptorSet 3
// CHECK-NEXT: OpDecorate %counter_var_exBindVsRegImpCt Binding 0

// implicit set vk::binding > :register implicit counter (counter part)
// CHECK:      OpDecorate %counter_var_impBindVsRegImpCt DescriptorSet 0
// CHECK-NEXT: OpDecorate %counter_var_impBindVsRegImpCt Binding 0

float4 main() : SV_Target {
    return 1.0;
}
