// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-reduce-msaa-to-single | %FileCheck %s

// Check that we overrode the sample index with 0 Here: -------------------------------------------------------------------V
// CHECK: %TextureLoad = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %texi_texture_2dMS, i32 0, i32 %4, i32 %5, i32 undef, i32 undef, i32 undef, i32 undef)

// Check for integer and half-float loads:
// CHECK: %TextureLoad1 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %texh_texture_2dMS, i32 0, i32 %add.i0, i32 %add.i1, i32 undef, i32 undef, i32 undef, i32 undef)
// CHECK: %TextureLoad2 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %tex_texture_2dMS, i32 0, i32 %add.i0, i32 %add.i1, i32 undef, i32 undef, i32 undef, i32 undef)

// Check texture load from single-sampled wasn't altered: (This should be 1) ---------------------------------------------------------V
// CHECK: %TextureLoad3 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %singleSampledTex_texture_2d, i32 1, i32 1, i32 1, i32 undef, i32 undef, i32 undef, i32 undef)

Texture2DMS<float4> tex : register(t2);
Texture2DMS<half4> texh : register(t3);
Texture2DMS<int4> texi : register(t4);
Texture2D<float4> singleSampledTex: register(t5);

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
  // Add a load from a single-sampled resource to check we didn't override that one's mip-level too:
  return resolved / samples + singleSampledTex.Load(int3(1,1,1));
}
