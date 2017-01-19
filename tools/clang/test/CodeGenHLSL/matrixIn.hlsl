// RUN: %dxc -E main -T ps_6_0 %s

float main(float4x4 a : A, int4 b : B) : SV_Target
{
  return a[b.x][b.y];
}
