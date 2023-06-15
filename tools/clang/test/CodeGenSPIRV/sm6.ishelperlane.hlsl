// RUN: %dxc -T ps_6_0 -E main

// CHECK-NOT: OpDecorate {{%\w+}} BuiltIn HelperInvocation

// CHECK: %gl_HelperInvocation = OpVariable %_ptr_Private_bool Input
float4 a;

float4 main(bool isHI
            : HI) : SV_Target {
  float4 result = a + IsHelperLane();
  return ddx(result);
}
// CHECK:      %module_init = OpFunction %void None
// CHECK:      %module_init_bb = OpLabel
// CHECK:      %gl_HelperInvocation = OpVariable %_ptr_Input_bool Input
// CHECK-NEXT: OpLoad %bool %gl_HelperInvocation

