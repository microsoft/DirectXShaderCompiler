// RUN: %dxc -E main -T ps_5_0 %s

float main(float2 a : A, int3 b : B) : SV_Target
{
  float r;
  [call]
  switch(b.x)
  {
  case 1:
    r = 5.f;
    break;
  case 2:
    r = a.x;
    break;
  default:
    r = 3.f;
    break;
  }
  return r;
}
