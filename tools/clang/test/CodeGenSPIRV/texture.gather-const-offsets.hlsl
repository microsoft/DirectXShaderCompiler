// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// Four constant offsets should select the texels DXIL selects: ConstOffsets
// entry i is HLSL offset i plus lane i's position in the 2x2 footprint,
// (0,1), (1,1), (1,0) and (0,0) for lanes x, y, z and w.

SamplerState gSampler;
SamplerComparisonState gCmpSampler;
Texture2D<float4> tex;

// CHECK-DAG: [[c01:%[0-9]+]] = OpConstantComposite %v2int %int_0 %int_1
// CHECK-DAG: [[c11:%[0-9]+]] = OpConstantComposite %v2int %int_1 %int_1
// CHECK-DAG: [[c10:%[0-9]+]] = OpConstantComposite %v2int %int_1 %int_0
// CHECK-DAG: [[c00:%[0-9]+]] = OpConstantComposite %v2int %int_0 %int_0
// CHECK-DAG: [[zeros:%[0-9]+]] = OpConstantComposite %_arr_v2int_uint_4 [[c01]] [[c11]] [[c10]] [[c00]]
// CHECK-DAG: [[cm10:%[0-9]+]] = OpConstantComposite %v2int %int_n1 %int_0
// CHECK-DAG: [[c0m1:%[0-9]+]] = OpConstantComposite %v2int %int_0 %int_n1
// CHECK-DAG: [[cm1m1:%[0-9]+]] = OpConstantComposite %v2int %int_n1 %int_n1
// CHECK-DAG: [[negs:%[0-9]+]] = OpConstantComposite %_arr_v2int_uint_4 [[cm10]] [[c00]] [[c0m1]] [[cm1m1]]

float4 main(float2 uv : TEXCOORD0, float ref : REF) : SV_Target {
// CHECK: OpImageGather %v4float {{%[0-9]+}} {{%[0-9]+}} %int_0 ConstOffsets [[zeros]]
  float4 a = tex.GatherRed(gSampler, uv, int2(0, 0), int2(0, 0), int2(0, 0), int2(0, 0));
// CHECK: OpImageGather %v4float {{%[0-9]+}} {{%[0-9]+}} %int_3 ConstOffsets [[negs]]
  float4 b = tex.GatherAlpha(gSampler, uv, int2(-1, -1), int2(-1, -1), int2(-1, -1), int2(-1, -1));
// CHECK: OpImageDrefGather %v4float {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} ConstOffsets [[zeros]]
  float4 c = tex.GatherCmpRed(gCmpSampler, uv, ref, int2(0, 0), int2(0, 0), int2(0, 0), int2(0, 0));
  return a + b + c;
}
