// RUN: %dxc -E main -T ps_6_0 %s

float4 main() : SV_Target {
  return float4(1, 0, 0, 1);
}
