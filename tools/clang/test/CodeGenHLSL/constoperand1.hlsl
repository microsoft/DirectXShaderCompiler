// RUN: %dxc -E main -T vs_6_0 %s

float4 main() : SV_POSITION
{
  return float4(3,0,0.5,0.12345);
}