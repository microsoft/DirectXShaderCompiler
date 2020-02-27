// RUN: %dxc -E main -T ps_6_0 %s -Od /Zi | FileCheck %s

float foo(float arg) {
  return arg;
}

float main() : SV_Target {
  // CHECK: %[[p_load:[0-9]+]] = load i32, i32* @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  float x = 10; // CHECK: %[[x:.+]] = select i1 %[[p]], float 1.000000e+01, float 1.000000e+01
  float y = foo(x); // CHECK: load i32, i32* @dx.nothing
  // CHECK: %[[arg:.+]] = select i1 %[[p]], float %[[x]], float %[[x]]
  // CHECK: %[[ret:.+]] = select i1 %[[p]], float %[[arg]], float %[[arg]]
  // CHECK: %[[ret2:.+]] = select i1 %[[p]], float %[[ret]], float %[[ret]]
  return y;
}

