// RUN: %dxc -Tlib_6_5 %s -verify 

// expected-error@+1{{recursive functions are not allowed: export function calls recursive function 'recurse2'}}
export void recurse2(inout float4 f, float a) {
  if (a > 0) {
    recurse2(f, a);
  }
  f -= abs(f+a);
}

// expected-error@+1{{recursive functions are not allowed: export function calls recursive function 'recurse'}}
export void recurse(inout float4 f, float a)
{
    if (a > 1) {
      recurse(f, a-1);
      recurse2(f, a-1);
    }
    f += abs(f+a);
}

float4 main(float a : A, float b:B) : SV_TARGET
{
  float4 f = b;
  recurse(f, a);
  return f;
}

