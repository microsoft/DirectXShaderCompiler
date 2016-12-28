// RUN: %dxc -E main -T cs_5_0 -fcgl %s  | FileCheck %s

// CHECK: float %a, <4 x float> %b, %struct.T* byval %t, %class.matrix.float.2.3 %m, [3 x <2 x float>]* byval %n

// CHECK: float* dereferenceable(4) %a, <4 x float>* dereferenceable(16) %b, %struct.T* %t, %class.matrix.float.2.3* dereferenceable(24) %m, [3 x <2 x float>]* %n

struct T{
  float a;
  float4 b;
};

float test(float a, float4 b, T t, float2x3 m, float2 n[3]) {
  return a + t.a;
}

float test2(out float a, out float4 b, out T t, out float2x3 m, out float2 n[3]) {
  return a + t.a;
}

[numthreads(8,8,1)]
void main() {
  float a = 1;
  float b = 2;
  T t;
  t.a = 1;
  t.b = 2;
  float2x3 m = 0;
  float2 n[3];
  n[0] = 0; n[1] = 1; n[2] = 2;
  test(a, b, t, m, n);
  // TODO: report error on use float as out float4 in front-end.
  // FXC error message is "cannot convert output parameter from 'float4' to 'float'"
  //test2(a, b, t, m, n);
}
