// RUN: %dxc -E main -T ps_6_0 %s

struct Input
{
    float2 v : TEXCOORD0;
};

float4 a;

float4 test(float b, float c) {
  float4 r = 0;
  if (c > 1) {
    r += 2;
    // This block will be removed because b is 1.
    // The use of a will be removed, but gep to a is not.
    if (b > 1)
       r += a;
  }

  return r;
}

float4 main(Input input) : SV_Target
{
    if ((input.v[1]) > 1)
       return test(1, input.v.x);
    return input.v[0];
}