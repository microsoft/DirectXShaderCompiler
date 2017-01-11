// RUN: %dxc -E main -T ps_6_0 %s

float4 main(float4 a : A) : SV_TARGET
{
  return -abs(a.yxxx);
}
