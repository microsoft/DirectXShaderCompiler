// RUN: %dxc -T ps_6_0 -HV 2018 %s | FileCheck %s
// RUN: %dxc -T ps_6_0 -HV 2018 -DSTATIC_GLOBAL %s | FileCheck %s

// Validate that a cast between two structs of identical layout whose leading
// members bottom out in an empty struct does not crash the compiler.

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)

struct Empty {};

struct Inner {
  Empty e;
};

struct S {
  Inner i;
  float a;
};

struct T {
  Inner i;
  float a;
};

void f(inout S s) {
  s.a = 1;
}

#ifdef STATIC_GLOBAL

static T g;

float4 main() : SV_Target {
  f(g);
  return g.a;
}

#else

float4 main() : SV_Target {
  T t = (T)0;
  f(t);
  return t.a;
}

#endif
