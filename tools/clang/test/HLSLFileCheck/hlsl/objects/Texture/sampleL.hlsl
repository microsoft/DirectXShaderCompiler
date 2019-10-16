// RUN: %dxc -E main -T ps_6_2 -enable-16bit-types %s   | FileCheck %s

SamplerState samp1 : register(s5);
Texture2D<float4> text1 : register(t3);
TextureCubeArray<float4> text2 : register(t5);
Texture2D<half4> text3 : register(t7);
TextureCubeArray<half4> text4 : register(t9);
int LOD;
float4 main(float2 a : A) : SV_Target
{
  uint status;
  float4 r = 0;

  // CHECK: call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62,
  r += text1.SampleLevel(samp1, a, LOD);
  // CHECK: call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62,
  r += text1.SampleLevel(samp1, a, LOD, uint2(-5, 7));
  // CHECK: call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62,
  // CHECK: call i1 @dx.op.checkAccessFullyMapped.i32(i32 71,
  r += text1.SampleLevel(samp1, a, LOD, uint2(-3, 2), status); r += CheckAccessFullyMapped(status);
  // CHECK: call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62,
  r += text2.SampleLevel(samp1, a.xyxy, LOD);
  // CHECK: call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62,
  // CHECK: call i1 @dx.op.checkAccessFullyMapped.i32(i32 71,
  r += text2.SampleLevel(samp1, a.xyxy, LOD * 0.5, status); r += CheckAccessFullyMapped(status);

  half4 h = 0;

  // CHECK: call %dx.types.ResRet.f16 @dx.op.sampleLevel.f16(i32 62,
  h += text3.SampleLevel(samp1, a, LOD);
  // CHECK: call %dx.types.ResRet.f16 @dx.op.sampleLevel.f16(i32 62,
  h += text3.SampleLevel(samp1, a, LOD, uint2(-5, 7));
  // CHECK: call %dx.types.ResRet.f16 @dx.op.sampleLevel.f16(i32 62,
  // CHECK: call i1 @dx.op.checkAccessFullyMapped.i32(i32 71,
  h += text3.SampleLevel(samp1, a, LOD, uint2(-3, 2), status); h += CheckAccessFullyMapped(status);
  // CHECK: call %dx.types.ResRet.f16 @dx.op.sampleLevel.f16(i32 62,
  h += text4.SampleLevel(samp1, a.xyxy, LOD);
  // CHECK: call %dx.types.ResRet.f16 @dx.op.sampleLevel.f16(i32 62,
  // CHECK: call i1 @dx.op.checkAccessFullyMapped.i32(i32 71,
  h += text4.SampleLevel(samp1, a.xyxy, LOD * 0.5, status); h += CheckAccessFullyMapped(status);
 
  return r + h;
}
