// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpEntryPoint Fragment %main "main" %gl_FragCoord %out_var_SV_Target

// CHECK:     %gl_FragCoord = OpVariable %_ptr_Input_v4float Input
// CHECK-NOT:      {{%[0-9]+}} = OpVariable %_ptr_Input_v4float Input

struct PSInput {
  float4 a : SV_Position;
  float4 b : SV_Position;
};

// CHECK:      [[a:%[0-9]+]] = OpLoad %v4float %gl_FragCoord
// CHECK-NEXT: [[b:%[0-9]+]] = OpLoad %v4float %gl_FragCoord
// CHECK-NEXT:   {{%[0-9]+}} = OpCompositeConstruct %PSInput [[a]] [[b]]

float4 main(PSInput input) : SV_Target
{
	return input.a + input.b;
}

