// RUN: %dxc -D PARAMTYPE=Tex2D /E main -T ps_6_0 %s | FileCheck %s
// RUN: %dxc -D PARAMTYPE=SamplerState /E main -T ps_6_0 %s | FileCheck %s
// RUN: %dxc -D PARAMTYPE=ResourceContainer /E main -T ps_6_0 %s | FileCheck %s

// CHECK: Resource or sampler parameter is not allowed on entry function.

#define Tex2D Texture2D<float4>
struct ResourceContainer {
	Texture2D<float4> texture_resource;
};
int main(float2 texCoord : TEXCOORD0, PARAMTYPE tex  : TEXUNIT0) : SV_Target {
  return 3;
}