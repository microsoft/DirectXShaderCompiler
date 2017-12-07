// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

// vk::binding + vk::counter_binding
[[vk::binding(5, 3), vk::counter_binding(10)]]
RWStructuredBuffer<S> mySBuffer1;
// CHECK:      OpDecorate %mySBuffer1 DescriptorSet 3
// CHECK-NEXT: OpDecorate %mySBuffer1 Binding 5

// CHECK:      OpDecorate %counter_var_mySBuffer1 DescriptorSet 3
// CHECK-NEXT: OpDecorate %counter_var_mySBuffer1 Binding 10

// :register + vk::counter_binding
[[vk::counter_binding(20)]]
AppendStructuredBuffer<S> myASBuffer1 : register(u1, space1);
// CHECK:      OpDecorate %counter_var_myASBuffer1 DescriptorSet 1
// CHECK-NEXT: OpDecorate %counter_var_myASBuffer1 Binding 20

// none + vk::counter_binding
[[vk::counter_binding(2)]]
ConsumeStructuredBuffer<S> myCSBuffer1;
// CHECK:      OpDecorate %counter_var_myCSBuffer1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %counter_var_myCSBuffer1 Binding 2

// vk::binding + none
[[vk::binding(1)]]
RWStructuredBuffer<S> mySBuffer2;
// CHECK:      OpDecorate %mySBuffer2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %mySBuffer2 Binding 1

// CHECK-NEXT: OpDecorate %myASBuffer1 DescriptorSet 1
// CHECK-NEXT: OpDecorate %myASBuffer1 Binding 1

// :register + none
AppendStructuredBuffer<S> myASBuffer2 : register(u3, space2);
// CHECK-NEXT: OpDecorate %myASBuffer2 DescriptorSet 2
// CHECK-NEXT: OpDecorate %myASBuffer2 Binding 3

// CHECK-NEXT: OpDecorate %myCSBuffer1 DescriptorSet 0
// CHECK-NEXT: OpDecorate %myCSBuffer1 Binding 0

// CHECK-NEXT: OpDecorate %counter_var_mySBuffer2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %counter_var_mySBuffer2 Binding 3

// CHECK-NEXT: OpDecorate %counter_var_myASBuffer2 DescriptorSet 2
// CHECK-NEXT: OpDecorate %counter_var_myASBuffer2 Binding 0

// none + none
ConsumeStructuredBuffer<S> myCSBuffer2;
// CHECK-NEXT: OpDecorate %myCSBuffer2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %myCSBuffer2 Binding 4

// CHECK-NEXT: OpDecorate %counter_var_myCSBuffer2 DescriptorSet 0
// CHECK-NEXT: OpDecorate %counter_var_myCSBuffer2 Binding 5

float4 main() : SV_Target {
    uint a = mySBuffer1.IncrementCounter();
    uint b = mySBuffer2.DecrementCounter();

    return  a + b;
}
