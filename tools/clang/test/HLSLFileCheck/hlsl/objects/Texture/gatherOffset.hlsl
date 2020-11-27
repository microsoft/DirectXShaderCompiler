// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 0
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 1
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 2
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 3
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 0
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 1
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 2
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 3
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 0
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 1
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 2
// CHECK: dx.op.textureGather.f32(i32 73
// CHECK: extractvalue {{.*}}, 3


SamplerState samp1;
Texture2D<float4> text1;
Texture2DArray<float4> text2;
TextureCubeArray<float4> text3;

float4 main(float4 a : A, float4 b : B) : SV_Target
{
  uint status;
  float4 r = 0;
  r += text1.GatherRed(samp1, a.xy, int2(0, 0), int2(-1, 1), int2(3, 4), int2(-8, 7));
  r += text1.GatherAlpha(samp1, a.xy, int2(0, 0), int2(-1, 1), int2(3, 4), int2(-8, 7), status); r += status;

  r += text2.GatherBlue(samp1, a.xyz, int2(0, 0), int2(-1, 1), int2(3, 4), int2(-8, 7), status); r += status;
  return r;
}
