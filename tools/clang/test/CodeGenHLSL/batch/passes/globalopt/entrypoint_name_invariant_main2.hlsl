// RUN: %dxc -T vs_6_0 -E main2 %s | FileCheck %s

// Regression test for #2318, where the globalopt had behavior
// conditional on the name of the main function.

static uint g[1] = { 0 };
uint main2() : OUT
{
  // CHECK: load i32
  // CHECK: add i32
  // CHECK: store i32
  g[0]++;
  // CHECK: call void @dx.op.storeOutput.i32
  return g[0];
}