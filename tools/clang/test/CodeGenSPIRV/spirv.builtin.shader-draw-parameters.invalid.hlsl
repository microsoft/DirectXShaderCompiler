// Run: %dxc -T ps_6_0 -E main

float4 main(
    [[vk::builtin("BaseVertex")]] float baseVertex : A
) : SV_Target {
    return baseVertex;
}

// CHECK: :4:7: error: BaseVertex builtin must be of 32-bit scalar integer type
// CHECK: :4:7: error: BaseVertex builtin can only be used in vertex shader input