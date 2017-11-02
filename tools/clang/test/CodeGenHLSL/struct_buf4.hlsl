// RUN: %dxc -E main -T ps_6_2 %s | FileCheck %s

// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf_struct_UAV_structbuf, i32 0, i32 20, i8 1)
// CHECK: extractvalue %dx.types.ResRet.f32 %RawBufferLoad, 0

struct MyStruct {
  float3x3 m;
};

RWStructuredBuffer<MyStruct> buf_struct;
float main() : SV_Target {
  return buf_struct[0].m[2][1];
}