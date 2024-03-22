// RUN: %dxc -E main -T ps_6_4 -enable-16bit-types -spirv %s | FileCheck %s

float main(half4 inputs : Inputs0, int acc : Acc0) : SV_Target {

// CHECK: [[inputs:%[0-9]+]] = OpLoad %v4half %in_var_Inputs0
// CHECK: [[acc:%[0-9]+]] = OpLoad %int %in_var_Acc0
// CHECK: [[b:%[0-9]+]] = OpVectorShuffle %v2half [[inputs]] [[inputs]] 2 3
// CHECK: [[a:%[0-9]+]] = OpVectorShuffle %v2half [[inputs]] [[inputs]] 0 1
// CHECK: [[dot:%[0-9]+]] = OpDot %half [[a]] [[b]]
// CHECK: [[dot2:%[0-9]+]] = OpFConvert %float [[dot]]
// CHECK: [[acc2:%[0-9]+]] = OpConvertSToF %float [[acc]]
// CHECK: [[res:%[0-9]+]] = OpFAdd %float [[dot2]] [[acc2]]
  return dot2add(inputs.xy, inputs.zw, acc);
}
