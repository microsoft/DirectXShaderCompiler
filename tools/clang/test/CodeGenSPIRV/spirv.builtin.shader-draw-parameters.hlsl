// Run: %dxc -T vs_6_0 -E main

// CHECK: OpCapability DrawParameters

// CHECK: OpExtension "SPV_KHR_shader_draw_parameters"


// CHECK: OpDecorate [[a:%\d+]] BuiltIn BaseVertex
// CHECK: OpDecorate [[b:%\w+]] BuiltIn BaseInstance
// CHECK: OpDecorate [[c:%\d+]] BuiltIn DrawIndex

float main(
// CHECK: [[a]] = OpVariable %_ptr_Input_int Input
    [[vk::builtin("BaseVertex")]]    int baseVertex : A,
// CHECK: [[b]] = OpVariable %_ptr_Input_uint Input
    [[vk::builtin("BaseInstance")]] uint baseInstance : B,
// CHECK: [[c]] = OpVariable %_ptr_Input_int Input
    [[vk::builtin("DrawIndex")]]     int drawIndex : C
) : OUTPUT {
    return baseVertex + baseInstance + drawIndex;
}
