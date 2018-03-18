// Run: %dxc -T vs_6_1 -E main -fspv-extension=SPV_KHR_shader_draw_parameters

float4 main(
    [[vk::builtin("BaseVertex")]]    int baseVertex : A,
                                     uint viewid    : SV_ViewID) : B {
    return baseVertex + viewid;
}

// CHECK: :5:55: error: SPIR-V extension 'SPV_KHR_multiview' required for SV_ViewID but not permitted to use
