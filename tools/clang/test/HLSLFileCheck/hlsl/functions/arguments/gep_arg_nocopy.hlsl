// RUN: %dxc -E main -T ps_6_0 -fcgl %s | FileCheck %s

// Make sure no memcpy generated for gep arg.
// CHECK-NOT:memcpy

struct ST {
   float4 a[32];
   int4 b[32];
};

ST st;
int ci;

float4 foo(float4 a[32], int i) {
  return a[i];
}

float4 bar(float4 a[32]) {
  return foo(a, ci);
}

float4 main() : SV_Target {
  return bar(st.a);
}