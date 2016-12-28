// RUN: %dxc -E main -T ps_5_0 %s

float3 main(float3 a : A) : SV_Target
{
  return saturate(a.xzx);
}
