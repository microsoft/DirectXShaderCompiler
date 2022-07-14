// RUN: %dxc -D PARAMTYPE=Tex2D /E main -T ps_6_0 %s | FileCheck %s -check-prefix=CHECK1
// CHECK1: Resource or sampler parameter is not allowed on entry function.

// RUN: %dxc -D PARAMTYPE=SamplerState /E main -T ps_6_0 %s | FileCheck %s -check-prefix=CHECK2
// CHECK2: Resource or sampler parameter is not allowed on entry function.

#define Tex2D Texture2D<float4>

int main(float2 texCoord : TEXCOORD0, PARAMTYPE tex  : TEXUNIT0) : SV_Target {
  return 3;
}