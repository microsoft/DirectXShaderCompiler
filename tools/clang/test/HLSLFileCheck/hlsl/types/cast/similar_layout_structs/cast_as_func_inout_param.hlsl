// RUN: %dxc -T ps_6_0 -HV 2018 %s | FileCheck %s
// RUN: %dxc -T ps_6_0 -HV 2018 -DSTATIC_GLOBAL %s | FileCheck %s

// Validate that passing a struct to an inout parameter of a differently laid
// out struct does not hang the compiler, and that the write-back of the inout
// argument survives, whether the argument is a local or a static global.

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)

struct S {
  float2 v;
};

struct T {
  float x, y;
};

void f(inout S s) {
  s.v.x = 1;
}

#ifdef STATIC_GLOBAL

static T g;

float4 main() : SV_Target {
  f(g);
  return g.x;
}

#else

float4 main() : SV_Target {
  T t = (T)0;
  f(t);
  return t.x;
}

#endif
