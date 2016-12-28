// RUN: %dxc -E main -T ps_5_0 %s

int cast(float a)
{
  return a;
}

float main(float a : A) : SV_Target
{
    return cast(a);
}