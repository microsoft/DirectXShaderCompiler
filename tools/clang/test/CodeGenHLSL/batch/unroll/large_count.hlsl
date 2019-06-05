// RUN: %dxc -Od -E main -T ps_6_0 %s | FileCheck %s
// CHECK: Could not unroll loop
// CHECK: To give an explicit unroll bound, use unroll(n)
// CHECK-NOT: @main

[RootSignature("")]
float main(float y : Y) : SV_Target {
  float x = 0;

  static const uint kLoopCount = 512;

  [unroll]
  for (uint i = 0; i < kLoopCount; ++i)
  {
    x = x * x + y;
  }
  return x;
}
