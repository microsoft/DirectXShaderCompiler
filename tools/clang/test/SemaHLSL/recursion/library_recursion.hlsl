// RUN: dxc -Tlib_6_5 -verify %s 

// expected-error@+1{{recursive functions are not allowed: entry function calls recursive function 'recurse'}}
void recurse(inout float4 f, float a) 
{
    if (a > 1)
      recurse(f, a-1);
    f = abs(f+a);
}

float4 main(float a : A, float b:B) : SV_TARGET
{
  float4 f = b;
  recurse(f, a);
  return f;
}
