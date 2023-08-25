// RUN: %dxc -T lib_6_5 %s | FileCheck %s

// Check that results of wave intrinsics are not re-used
// cross DXR repacking points such as TraceRay() or CallShader();
struct Payload {
  unsigned int value;
};

[shader("miss")] void Miss(inout Payload p) {
  // Some runtime value that we can't reason about.
  // Freeze the current value and use it as input to
  // all intrinsics so that we know the intrinsics' result
  // would be the same before and after the CallShader call,
  // except for repacking.
  unsigned int oldValue = p.value;

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 0
  // CHECK: call i1 @dx.op.waveIsFirstLane(i32 110
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 0
  // CHECK: call i1 @dx.op.waveIsFirstLane(i32 110
  CallShader(0, p);
  p.value += WaveIsFirstLane();
  CallShader(0, p);
  p.value += WaveIsFirstLane();

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 1
  // CHECK: call i32 @dx.op.waveGetLaneIndex(i32 111
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 1
  // TODO: The second call is missing, we are incorrectly re-using the result
  //       from before the repacking point.
  // CHECK-NOT: call i32 @dx.op.waveGetLaneIndex(i32 111
  CallShader(1, p);
  p.value += WaveGetLaneIndex();
  CallShader(1, p);
  p.value += WaveGetLaneIndex();

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 2
  // CHECK: call i1 @dx.op.waveAnyTrue(i32 113, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 2
  // CHECK: call i1 @dx.op.waveAnyTrue(i32 113, i1
  CallShader(2, p);
  p.value += WaveActiveAnyTrue(oldValue == 17);
  CallShader(2, p);
  p.value += WaveActiveAnyTrue(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 3
  // CHECK: call i1 @dx.op.waveAllTrue(i32 114, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 3
  // CHECK: call i1 @dx.op.waveAllTrue(i32 114, i1
  CallShader(3, p);
  p.value += WaveActiveAllTrue(oldValue == 17);
  CallShader(3, p);
  p.value += WaveActiveAllTrue(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 4
  // CHECK: call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 4
  // CHECK: call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32
  CallShader(4, p);
  p.value += WaveActiveAllEqual(oldValue);
  CallShader(4, p);
  p.value += WaveActiveAllEqual(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 5
  // CHECK: call %dx.types.fouri32 @dx.op.waveActiveBallot(i32 116, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 5
  // CHECK: call %dx.types.fouri32 @dx.op.waveActiveBallot(i32 116, i1
  CallShader(5, p);
  p.value += WaveActiveBallot(oldValue).x;
  CallShader(5, p);
  p.value += WaveActiveBallot(oldValue).x;

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 6
  // CHECK: call i32 @dx.op.waveReadLaneAt.i32(i32 117, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 6
  // CHECK: call i32 @dx.op.waveReadLaneAt.i32(i32 117, i32
  CallShader(6, p);
  p.value += WaveReadLaneAt(oldValue, 1);
  CallShader(6, p);
  p.value += WaveReadLaneAt(oldValue, 1);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 7
  // CHECK: call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 7
  // CHECK: call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32
  CallShader(7, p);
  p.value += WaveReadLaneFirst(oldValue);
  CallShader(7, p);
  p.value += WaveReadLaneFirst(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 8
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 8
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  CallShader(8, p);
  p.value += WaveActiveSum(oldValue);
  CallShader(8, p);
  p.value += WaveActiveSum(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 9
  // CHECK: call i64 @dx.op.waveActiveOp.i64(i32 119, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 9
  // CHECK: call i64 @dx.op.waveActiveOp.i64(i32 119, i64
  CallShader(9, p);
  p.value += WaveActiveProduct(oldValue == 17 ? 1 : 0);
  CallShader(9, p);
  p.value += WaveActiveProduct(oldValue == 17 ? 1 : 0);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 10
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 10
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  CallShader(10, p);
  p.value += WaveActiveBitAnd(oldValue);
  CallShader(10, p);
  p.value += WaveActiveBitAnd(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 11
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 11
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  CallShader(11, p);
  p.value += WaveActiveBitXor(oldValue);
  CallShader(11, p);
  p.value += WaveActiveBitXor(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 12
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 12
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  CallShader(12, p);
  p.value += WaveActiveMin(oldValue);
  CallShader(12, p);
  p.value += WaveActiveMin(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 13
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 13
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  CallShader(13, p);
  p.value += WaveActiveMax(oldValue);
  CallShader(13, p);
  p.value += WaveActiveMax(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 14
  // CHECK: call i32 @dx.op.wavePrefixOp.i32(i32 121, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 14
  // CHECK: call i32 @dx.op.wavePrefixOp.i32(i32 121, i32
  CallShader(14, p);
  p.value += WavePrefixSum(oldValue);
  CallShader(14, p);
  p.value += WavePrefixSum(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 15
  // CHECK: call i64 @dx.op.wavePrefixOp.i64(i32 121, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 15
  // CHECK: call i64 @dx.op.wavePrefixOp.i64(i32 121, i64
  CallShader(15, p);
  p.value += WavePrefixProduct(oldValue == 17 ? 1 : 0);
  CallShader(15, p);
  p.value += WavePrefixProduct(oldValue == 17 ? 1 : 0);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 16
  // CHECK: call i32 @dx.op.waveAllOp(i32 135, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 16
  // CHECK: call i32 @dx.op.waveAllOp(i32 135, i1
  CallShader(16, p);
  p.value += WaveActiveCountBits(oldValue == 17);
  CallShader(16, p);
  p.value += WaveActiveCountBits(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 17
  // CHECK: call i32 @dx.op.wavePrefixOp(i32 136, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 17
  // CHECK: call i32 @dx.op.wavePrefixOp(i32 136, i1
  CallShader(17, p);
  p.value += WavePrefixCountBits(oldValue == 17);
  CallShader(17, p);
  p.value += WavePrefixCountBits(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 18
  // CHECK: call %dx.types.fouri32 @dx.op.waveMatch.i32(i32 165, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 18
  // CHECK: call %dx.types.fouri32 @dx.op.waveMatch.i32(i32 165, i32
  CallShader(18, p);
  uint4 mask = WaveMatch(oldValue);
  CallShader(18, p);
  p.value += WaveMatch(oldValue).x;

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 19
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 19
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  CallShader(19, p);
  p.value += WaveMultiPrefixBitAnd(oldValue, mask);
  CallShader(19, p);
  p.value += WaveMultiPrefixBitAnd(oldValue, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 20
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 20
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  CallShader(20, p);
  p.value += WaveMultiPrefixBitOr(oldValue, mask);
  CallShader(20, p);
  p.value += WaveMultiPrefixBitOr(oldValue, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 21
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 21
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  CallShader(21, p);
  p.value += WaveMultiPrefixBitXor(oldValue, mask);
  CallShader(21, p);
  p.value += WaveMultiPrefixBitXor(oldValue, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 22
  // CHECK: call i32 @dx.op.waveMultiPrefixBitCount(i32 167, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 22
  // CHECK: call i32 @dx.op.waveMultiPrefixBitCount(i32 167, i1
  CallShader(22, p);
  p.value += WaveMultiPrefixCountBits(oldValue == 17, mask);
  CallShader(22, p);
  p.value += WaveMultiPrefixCountBits(oldValue == 17, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 23
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 23
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  CallShader(23, p);
  p.value += WaveMultiPrefixProduct(oldValue == 17 ? 1 : 0, mask);
  CallShader(23, p);
  p.value += WaveMultiPrefixProduct(oldValue == 17 ? 1 : 0, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 24
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 24
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  CallShader(24, p);
  p.value += WaveMultiPrefixSum(oldValue == 17 ? 1 : 0, mask);
  CallShader(24, p);
  p.value += WaveMultiPrefixSum(oldValue == 17 ? 1 : 0, mask);
}
