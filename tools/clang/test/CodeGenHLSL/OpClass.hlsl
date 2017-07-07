// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Make sure WavePrefixSum and WavePrefixCountBits use different function.
// CHECK: dx.op.wavePrefixOp.i64(i32 121
// CHECK: dx.op.waveBitCount(i32 135
// CHECK: dx.op.waveBitCount(i32 136

float4 main() : SV_TARGET {
  uint3 u3 = { 1, 2, 3 };
  u3 += WavePrefixSum(3);

  uint u = 0;
  u += WaveActiveCountBits(1);
  u += WavePrefixCountBits(1);
  return u + u3.xyzx;
}