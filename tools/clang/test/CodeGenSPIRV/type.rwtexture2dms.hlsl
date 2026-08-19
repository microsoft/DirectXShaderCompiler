// RUN: %dxc -T ps_6_7 -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability StorageImageMultisample
// CHECK-DAG: OpTypeImage %float 2D 2 0 1 2 Rgba32f
// CHECK-DAG: OpTypeImage %uint 2D 2 1 1 2 R32ui

RWTexture2DMS<float4, 8> texture2dms;
RWTexture2DMSArray<uint, 8> texture2dmsArray;

void main() : SV_Target {}
