// RUN: %dxc -E main -T ps_6_0 %s

float4 c;

float4 GetCB() {
  return c;
}

uint n;

float4 main() : SV_TARGET
{
  float4 g=0;
  for (int i=0;i<n;i++) {
    g += GetCB();
  }
  return g;
}
