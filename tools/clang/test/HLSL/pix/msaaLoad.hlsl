// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-reduce-msaa-to-single | %FileCheck %s

// Check that we overrode the sample index with 0 Here: -------------------------------------------------------------------------------V
// CHECK: %TextureLoad = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %texi_texture_2dMS, i32 %i.015, i32 0, i32 %5, i32 undef, i32 undef, i32 undef, i32 undef)

// Check for integer and half-float loads:
// CHECK: %TextureLoad1 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %texh_texture_2dMS, i32 %i.015, i32 0, i32 %add.i1, i32 undef, i32 undef, i32 undef, i32 undef)
// CHECK: %TextureLoad2 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %tex_texture_2dMS, i32 %i.015, i32 0, i32 %add.i1, i32 undef, i32 undef, i32 undef, i32 undef)

Texture2DMS<float4> tex : register(t2);
Texture2DMS<half4> texh : register(t3);
Texture2DMS<int4> texi : register(t4);

struct PSInput
{
  float4 position : SV_POSITION;
  float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
  uint width, height, samples;
  tex.GetDimensions(width, height, samples);

  float4 resolved = float4(0, 0, 0, 0);
  for (uint i = 0; i < samples; ++i)
  {
    int2 iPos = int2(input.position.xy);

    //nonsensical loads from half and integer, just for test:
    iPos += texi.Load(iPos, i).x;
    resolved.g += texh.Load(iPos, i).g;

    resolved += tex.Load(iPos, i);
  }
  return resolved / samples;
}
