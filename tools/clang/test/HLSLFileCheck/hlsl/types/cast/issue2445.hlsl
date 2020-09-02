// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK:define void @main

// This checks for the absence of a bug reported in issue #2445 "Implicit truncation of vector to constant causes crash"
// The behavior varied depending on the init values of the float4 constructor.
// (0,0,0,0) => core dumped
// (1,0,0,0) => compiled successfully
// (0,1,0,0) => asserted in include/llvm/Support/Casting.h
// It is assumed that if the compilation succeeds the bug is not present.

float4 main() : SV_Target0
{
  const float e1 = float4(0,0,0,0);
  const float e2 = float4(1,0,0,0);
  const float e3 = float4(0,1,0,0);
  return e1 + e2 + e3;
}
