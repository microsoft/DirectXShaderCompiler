// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// Validate that passing a static global to an inout parameter of a differently
// laid out struct is diagnosed rather than hanging the compiler, when the
// global is used from an exported library function.

// CHECK: error: Unsupported cast between struct types with different layouts.

struct S {
  float2 v;
};

struct T {
  float x, y;
};

static T g;

void f(inout S s) {
  s.v.x = 1;
}

export float4 fn(float y) {
  g.y = y;
  f((S)g);
  return float4(g.x, g.y, 0, 0);
}
