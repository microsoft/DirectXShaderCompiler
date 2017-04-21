// RUN: %dxc -E main -T ps_6_0  -O0 %s | FileCheck %s

// CHECK: main


struct X {
  float2 a[2];
  float4 f;
};

X x0;
X x1;

void test_inout(inout X x, float idx)
{
   x.f = idx;
   if (x.f.x > 9)
   {
      x = x1;
   }
}

float4 main(float4 a : A, float4 b:B) : SV_TARGET
{
  X x = x0;
  test_inout(x, b.x);
  test_inout(x, a.y);
  return x.f + x.a[a.x].xyxy;
}

