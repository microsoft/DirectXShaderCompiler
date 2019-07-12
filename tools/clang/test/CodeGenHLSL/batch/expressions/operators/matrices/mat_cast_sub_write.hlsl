// RUN: %dxc /T ps_6_0 /E main %s | FileCheck %s

// Make sure cast then subscript works.

float4x4 a;
struct M {
  float4x3 a;
};

RWStructuredBuffer<M> buf;

float3 main(uint i:I, float3 x:X) :SV_Target {
  // Make sure if match no cast version.
  // buf[i].a[1].xyz = x;
  // CHECK:call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %buf_UAV_structbuf, i32 %{{[0-9]+}}, i32 4
  // CHECK:call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %buf_UAV_structbuf, i32 %{{[0-9]+}}, i32 20
  // CHECK:call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %buf_UAV_structbuf, i32 %{{[0-9]+}}, i32 36

  ((float3x3)buf[i].a)[1] = x;
  // a[1].xyz + a._m21_m20_m02;
  // CHECK:call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %"$Globals_cbuffer", i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  // CHECK:extractvalue %dx.types.CBufRet.f32 %{{[0-9]+}}, 1
  // CHECK:call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %"$Globals_cbuffer", i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  // CHECK:extractvalue %dx.types.CBufRet.f32 %{{[0-9]+}}, 1
  // CHECK:call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %"$Globals_cbuffer", i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  // CHECK:extractvalue %dx.types.CBufRet.f32 %{{[0-9]+}}, 1
  // CHECK:extractvalue %dx.types.CBufRet.f32 %{{[0-9]+}}, 2
  // CHECK:extractvalue %dx.types.CBufRet.f32 %{{[0-9]+}}, 2
  // CHECK:extractvalue %dx.types.CBufRet.f32 %{{[0-9]+}}, 0
  return ((float3x3)a)[1] + ((float3x3)a)._m21_m20_m02;
}