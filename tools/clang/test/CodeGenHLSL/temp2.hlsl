// RUN: %dxc -E main -T ps_5_0 %s

float4 main(int4 a : A, int4 b : B) : SV_TARGET
{
  int4 c = a + b;
  return c + int4(0,1,2,3);
}
