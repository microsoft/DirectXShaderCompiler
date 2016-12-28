// RUN: %dxc -E main -T ps_5_0 %s

float4 main(int4 b:B) : SV_TARGET
{
  float2x2 a = float2x2(0.1f, 0.2f, 0.3f, 0.4f);
  float2x2 c[2][2] = {a, 1.2, 1.1, b.xy, a, a};
  float2x2 d[2] = {c[b.z]};
  c[b.w] = d;
  float2x2 e[2] = c[b.z];
  return a[b.x][b.y] + c[b.x][b.x][b.y][b.y];
}

