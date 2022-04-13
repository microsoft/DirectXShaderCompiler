// RUN: %dxc -E main -T ps_6_0 %s -ast-dump | FileCheck %s

// CHECK: VarDecl {{0x[0-9a-fA-F]+}} {{<.*>}} col:14 used Tex 'Texture2D<vector<float, 4> >'

Texture2D    Tex;
SamplerState Samp;

float4 main(uint val : A) : SV_Target
{
  return Tex.Sample(Samp, float2(0.1, 0.2));
}
