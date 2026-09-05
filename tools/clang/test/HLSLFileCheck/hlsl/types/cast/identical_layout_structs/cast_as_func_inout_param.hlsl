// RUN: %dxc -T ps_6_0 -HV 2018 %s | FileCheck %s
// RUN: %dxc -T ps_6_0 -HV 2018 -DSTATIC_GLOBAL %s | FileCheck %s

// Validate that the copy-out of an inout argument survives a cast between two
// structs of identical layout.

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 2.000000e+00)

struct S {
  float a;
  int b;
};

struct T {
  float c;
  int d;
};

void f(inout S s) {
  s.a = 1;
  s.b = 2;
}

#ifdef STATIC_GLOBAL

static T g;

float4 main() : SV_Target {
  f(g);
  return float4(g.c, g.d, 0, 0);
}

#else

float4 main() : SV_Target {
  T t = (T)0;
  f(t);
  return float4(t.c, t.d, 0, 0);
}

#endif
