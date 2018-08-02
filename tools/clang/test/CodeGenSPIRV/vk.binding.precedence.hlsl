// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

// Verify that descriptor set-indicating override per: vk::binding > :register and
// :register > vk::set, using variations on these and vk::counter_binding

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

// :register > vk::set explicit counter (counter part)
[[vk::set(3), vk::counter_binding(2)]]
ConsumeStructuredBuffer<S> setVsRegExCt :register(u9, space6);
// CHECK:      OpDecorate %counter_var_setVsRegExCt DescriptorSet 6
// CHECK-NEXT: OpDecorate %counter_var_setVsRegExCt Binding 2

// :register > vk::set explicit counter (main part)
// CHECK-NEXT: OpDecorate %setVsRegExCt DescriptorSet 6
// CHECK-NEXT: OpDecorate %setVsRegExCt Binding 9

// :register > vk::set
[[vk::set(1)]]
Buffer<int> SetVsReg : register(t2, space3);
// CHECK:      OpDecorate %SetVsReg DescriptorSet 3
// CHECK-NEXT: OpDecorate %SetVsReg Binding 2

// :register > vk::set implicit counter (main part)
[[vk::set(3)]]
AppendStructuredBuffer<S> setVsRegImpCt : register(u6, space2);
// CHECK-NEXT: OpDecorate %setVsRegImpCt DescriptorSet 2
// CHECK-NEXT: OpDecorate %setVsRegImpCt Binding 6

// explicit set vk::binding > :register implicit counter (counter part)
// CHECK:      OpDecorate %counter_var_exBindVsRegImpCt DescriptorSet 3
// CHECK-NEXT: OpDecorate %counter_var_exBindVsRegImpCt Binding 0

// implicit set vk::binding > :register implicit counter (counter part)
// CHECK:      OpDecorate %counter_var_impBindVsRegImpCt DescriptorSet 0
// CHECK-NEXT: OpDecorate %counter_var_impBindVsRegImpCt Binding 0

// :register > vk::set implicit counter (counter part)
// CHECK-NEXT: OpDecorate %counter_var_setVsRegImpCt DescriptorSet 2
// CHECK-NEXT: OpDecorate %counter_var_setVsRegImpCt Binding 0

float4 main() : SV_Target {
    return 1.0;
}
