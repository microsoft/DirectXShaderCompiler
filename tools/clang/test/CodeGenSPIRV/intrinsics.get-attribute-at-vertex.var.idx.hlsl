// RUN: %dxc -T ps_6_0 -E main %s -spirv | FileCheck %s

struct PSInput {
  int idx: INPUT0;
  nointerpolation      float4 fp_h: FPH;
};

float4 main(PSInput input) : SV_Target {
// CHECK: [[idx:%[0-9]+]] = OpLoad %int %in_var_INPUT0
// CHECK: [[bc:%[0-9]+]] = OpBitcast %uint [[idx]]
// CHECK: [[ac:%[0-9]+]] = OpAccessChain %_ptr_Input_v4float %in_var_FPH [[bc]]
// CHECK: [[ld:%[0-9]+]] = OpLoad %v4float [[ac]]
// CHECK: OpStore %out_var_SV_Target [[ld]]
  return GetAttributeAtVertex(input.fp_h, input.idx);
}