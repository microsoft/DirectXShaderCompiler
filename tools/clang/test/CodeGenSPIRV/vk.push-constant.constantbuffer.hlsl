// Run: %dxc -T ps_6_0 -E main

struct StructA
{
    float3 one;
    float3 two;
};

// CHECK: %PushConstants = OpVariable %_ptr_PushConstant_type_PushConstant_ConstantBuffer PushConstant
[[vk::push_constant]] ConstantBuffer<StructA> PushConstants;

void main()
{
}
