// RUN: %dxc -T ps_6_7 -fcgl %s -spirv | FileCheck %s

// CHECK-DAG: OpCapability ImageMSArray
// CHECK-DAG: OpCapability StorageImageMultisample
// CHECK-DAG: OpTypeImage %float 2D 2 0 1 2 Rgba32f
// CHECK-DAG: OpTypeImage %uint 2D 2 1 1 2 R32ui
// CHECK: OpImageRead %v4float {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK: OpImageRead %v4uint {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK: OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}
// CHECK: OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample {{%[0-9]+}}

RWTexture2DMS<float4, 8> texture2dms;
RWTexture2DMSArray<uint, 8> texture2dmsArray;

float4 main(uint2 coord : TEXCOORD0, uint sample : TEXCOORD1) : SV_Target {
  float4 value = texture2dms.Load(coord, sample);
  value += texture2dmsArray.Load(uint3(coord, 0), sample);

  texture2dms.sample[sample][coord] = value;
  texture2dmsArray.sample[sample][uint3(coord, 0)] = value.x;
  return value;
}
