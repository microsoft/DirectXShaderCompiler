// RUN: %dxc -E main -T ps_5_0 %s

float main(int a : A) : SV_Target
{
  return a;
}
