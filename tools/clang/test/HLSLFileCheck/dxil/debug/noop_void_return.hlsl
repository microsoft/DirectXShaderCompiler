// RUN: %dxc -preserve-intermediate-values -E main -T ps_6_0 %s -Od | FileCheck %s

static float my_glob;

void foo() {
  my_glob = 10;
  return;
}

[RootSignature("")]
float main() : SV_Target {
  // CHECK: %[[p_load:[0-9]+]] = load i32, i32*
  // CHECK-SAME: @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  // CHECK: select i1 %[[p]]
  my_glob = 0;

  // Function call
  // CHECK: load i32, i32*
  // CHECK: @dx.nothing
  foo();
    // CHECK: select i1 %[[p]]
    // void return

  return my_glob;
}



