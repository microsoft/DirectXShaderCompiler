// RUN: %dxc -E main -T ps_5_0 %s

float4 g1;

float4 main() : SV_TARGET
{
  return g1.wyyy;
}
