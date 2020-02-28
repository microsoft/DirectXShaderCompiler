// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

struct S {
  float x;
  float y;
};

void foo(out S arg) {
  arg.x = 20;
  arg.y = 30;
  return;
}

void bar(inout S arg) {
  arg.x *= 2;
  arg.y *= 3;
  return;
}

void baz(inout float x, inout float y) {
  x *= 0.5;
  y *= 0.5;
  return;
}

[RootSignature("")]
float main() : SV_Target {
  // CHECK: %[[p_load:[0-9]+]] = load i32, i32* @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  S s;

  // CHECK: load i32, i32* @dx.nothing
  foo(s);
    // CHECK: select i1 %[[p]]
    // CHECK: select i1 %[[p]]
    // CHECK: load i32, i32* @dx.nothing

  // CHECK: load i32, i32* @dx.nothing
  bar(s);
    // CHECK: fmul
    // CHECK: fmul
    // CHECK: load i32, i32* @dx.nothing

  // CHECK: load i32, i32* @dx.nothing
  baz(s.x, s.y);
    // CHECK: fmul
    // CHECK: fmul
    // CHECK: load i32, i32* @dx.nothing

  // CHECK: fadd
  return s.x + s.y;
}



