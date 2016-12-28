// RUN: %dxc -E main -T ps_5_0 %s

float main(uint a : A) : SV_Target
{
  return a;
}
