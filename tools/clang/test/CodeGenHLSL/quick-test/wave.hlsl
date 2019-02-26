// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: Wave level operations
// CHECK: waveIsFirstLane
// CHECK: waveGetLaneIndex
// CHECK: waveGetLaneCount
// CHECK: waveAnyTrue
// CHECK: waveAllTrue
// CHECK: waveActiveAllEqual
// CHECK: waveActiveBallot
// CHECK: waveReadLaneAt
// CHECK: waveReadLaneFirst
// CHECK: waveActiveOp
// CHECK: waveActiveOp
// CHECK: waveActiveBit
// CHECK: waveActiveBit
// CHECK: waveActiveBit
// CHECK: waveActiveOp
// CHECK: waveActiveOp
// CHECK: quadReadLaneAt
// CHECK: quadOp
// CHECK: quadOp

float4 main() : SV_TARGET {
  float f = 1;
  if (WaveIsFirstLane()) {
    f += 1;
  }
  f += WaveGetLaneIndex();
  if (WaveGetLaneCount() == 0) {
    f += 1;
  }
  if (WaveActiveAnyTrue(true)) {
    f += 1;
  }
  if (WaveActiveAllTrue(true)) {
    f += 1;
  }
  if (WaveActiveAllEqual(WaveGetLaneIndex())) {
    f += 1;
  }
  uint4 val = WaveActiveBallot(true);
  if (val.x == 1) {
    f += 1;
  }
  float3 f3 = { 1, 2, 3 };
  uint3 u3 = { 1, 2 ,3 };
  uint u = 0;
  uint2 u2 = { 1, 2 };
  int i_signed = -2;
  f += WaveReadLaneAt(f3, 1).x;
  f3 += WaveReadLaneFirst(f3).x;
  f3 += WaveActiveSum(f3).x;
  f3 += WaveActiveProduct(f3).x;
  u3 += WaveActiveBitAnd(u3);
  u3 += WaveActiveBitOr(u3);
  u3 += WaveActiveBitXor(u3);
  u3 += WaveActiveMin(u3);
  u3 += WaveActiveMax(u3);
  u3 += abs(WaveActiveMin(i_signed));
  u3 += abs(WaveActiveMax(i_signed));
  f3 += WavePrefixSum(3);
  f3 += WavePrefixProduct(3);
  f3 = QuadReadLaneAt(f3, 1);
  u += QuadReadAcrossX(u);
  u2 = QuadReadAcrossY(u2);
  u2 = QuadReadAcrossDiagonal(u2);
  u += WaveActiveCountBits(1);
  u += WavePrefixCountBits(1);
  return f + f3.x + u + u2.x + u3.y;
}
