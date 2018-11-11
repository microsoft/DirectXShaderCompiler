// Run: %dxc -T ps_6_0 -E main

// CHECK: OpEntryPoint Fragment %main "main" %gl_FragCoord %out_var_SV_Target

// CHECK:     %gl_FragCoord = OpVariable %_ptr_Input_v4float Input
// CHECK-NOT:      {{%\d+}} = OpVariable %_ptr_Input_v4float Input

struct PSInput {
  float4 a : SV_Position;
  float4 b : SV_Position;
};

// CHECK:      [[a:%\d+]] = OpLoad %v4float %gl_FragCoord
// CHECK-NEXT: [[b:%\d+]] = OpLoad %v4float %gl_FragCoord
// CHECK-NEXT:   {{%\d+}} = OpCompositeConstruct %PSInput [[a]] [[b]]

float4 main(PSInput input) : SV_Target
{
	return input.a + input.b;
}

