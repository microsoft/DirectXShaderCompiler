// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: Recursion is not permitted
// CHECK: Function call on user defined function

void test_inout(inout float4 m, float4 a) 
{
    if (a.x > 1)
      test_inout(m, a-1);
    m = abs(m+a*a.yxxx);
}

float4 main(float4 a : A, float4 b:B) : SV_TARGET
{
  float4 x = b;
  test_inout(x, a);
  return x;
}

