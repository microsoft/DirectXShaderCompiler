// RUN: %dxc -T ps_6_0 -E main

// CHECK-NOT: OpDecorate {{%\w+}} BuiltIn HelperInvocation

// CHECK: %HelperInvocation = OpVariable %_ptr_Private_bool Private

float4 main([[vk::builtin("HelperInvocation")]] bool isHI : HI) : SV_Target {
// CHECK:      [[val:%\d+]] = OpLoad %bool %HelperInvocation
// CHECK-NEXT: OpStore %param_var_isHI [[val]]
    float ret = 1.0;

    if (isHI) ret = 2.0;

    return ret;
}
// CHECK:      %module_init = OpFunction %void None
// CHECK:      %module_init_bb = OpLabel
// CHECK:      [[HelperInvocation:%\d+]] = OpIsHelperInvocationEXT %bool
// CHECK-NEXT: OpStore %HelperInvocation [[HelperInvocation]]
