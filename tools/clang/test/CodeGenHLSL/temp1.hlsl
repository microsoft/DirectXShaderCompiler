// RUN: %dxc -E main -T ps_6_0 %s

float4 main(float4 a : A, float4 b : B) : SV_TARGET
{
  return a + b + float4(0,1,2,3);
}
