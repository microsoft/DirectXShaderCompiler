// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan -fcgl %s -spirv | FileCheck %s

// CHECK:          [[set:%[0-9]+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK:   [[float_name:%[0-9]+]] = OpString "float"
// CHECK:       [[empty:%[0-9]+]] = OpString ""
// CHECK:      [[a_name:%[0-9]+]] = OpString "a"
// CHECK:      [[b_name:%[0-9]+]] = OpString "b"
// CHECK: [[globals_type_name:%[0-9]+]] = OpString "type.$Globals"
// CHECK:         [[float_ty:%[0-9A-Za-z_\.]+]] = OpTypeFloat 32
// CHECK:       [[globals_ty:%[0-9A-Za-z_\.]+]] = OpTypeStruct [[float_ty]] [[float_ty]]
// CHECK:      [[globals_var:%[0-9A-Za-z_\.]+]] = OpVariable {{%[0-9A-Za-z_\.]+}} Uniform
// CHECK:        [[float_dbg:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeBasic [[float_name]] %uint_32 %uint_3 %uint_0
// CHECK:         [[a_member:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeMember [[a_name]] [[float_dbg]] {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} %uint_0 %uint_32 %uint_3
// CHECK:         [[b_member:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeMember [[b_name]] [[float_dbg]] {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} %uint_32 %uint_32 %uint_3
// CHECK:    [[globals_dbg:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeComposite [[globals_type_name]] %uint_1 {{%[0-9]+}} %uint_0 %uint_0 {{%[0-9]+}} [[globals_type_name]] %uint_64 %uint_3 [[a_member]] [[b_member]]
// CHECK-COUNT-1: {{%[0-9]+}} = OpExtInst %void [[set]] DebugGlobalVariable [[empty]] [[globals_dbg]] {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} {{%[0-9A-Za-z_\.]+}} [[empty]] [[globals_var]] %uint_8

uniform float a;
uniform float b;

float4 main(float4 color : COLOR) : SV_TARGET {
  return color + (a + b).xxxx;
}
