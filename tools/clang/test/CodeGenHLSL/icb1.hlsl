// RUN: %dxc -E main -T ps_5_0 %s

// TODO: change float4 a[4] to float4x4 a
float main(float4 a[4] : A, int4 b : B) : SV_Target
{
  return a[b.x][b.y];
}
