// RUN: %dxc -T lib_6_5 %s | FileCheck %s

// Check that results of wave intrinsics are not re-used
// cross DXR repacking points such as TraceRay() or CallShader();
struct Payload {
  unsigned int value;
};

void RepackingPoint(int identifier) {
  Payload p;
  CallShader(identifier, p);
}

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
  RepackingPoint(0);
  p.value += WaveIsFirstLane();
  RepackingPoint(0);
  p.value += WaveIsFirstLane();

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 1
  // CHECK: call i32 @dx.op.waveGetLaneIndex(i32 111
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 1
  // CHECK: call i32 @dx.op.waveGetLaneIndex(i32 111
  RepackingPoint(1);
  p.value += WaveGetLaneIndex();
  RepackingPoint(1);
  p.value += WaveGetLaneIndex();

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 2
  // CHECK: call i1 @dx.op.waveAnyTrue(i32 113, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 2
  // CHECK: call i1 @dx.op.waveAnyTrue(i32 113, i1
  RepackingPoint(2);
  p.value += WaveActiveAnyTrue(oldValue == 17);
  RepackingPoint(2);
  p.value += WaveActiveAnyTrue(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 3
  // CHECK: call i1 @dx.op.waveAllTrue(i32 114, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 3
  // CHECK: call i1 @dx.op.waveAllTrue(i32 114, i1
  RepackingPoint(3);
  p.value += WaveActiveAllTrue(oldValue == 17);
  RepackingPoint(3);
  p.value += WaveActiveAllTrue(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 4
  // CHECK: call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 4
  // CHECK: call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32
  RepackingPoint(4);
  p.value += WaveActiveAllEqual(oldValue);
  RepackingPoint(4);
  p.value += WaveActiveAllEqual(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 5
  // CHECK: call %dx.types.fouri32 @dx.op.waveActiveBallot(i32 116, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 5
  // CHECK: call %dx.types.fouri32 @dx.op.waveActiveBallot(i32 116, i1
  RepackingPoint(5);
  p.value += WaveActiveBallot(oldValue).x;
  RepackingPoint(5);
  p.value += WaveActiveBallot(oldValue).x;

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 6
  // CHECK: call i32 @dx.op.waveReadLaneAt.i32(i32 117, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 6
  // CHECK: call i32 @dx.op.waveReadLaneAt.i32(i32 117, i32
  RepackingPoint(6);
  p.value += WaveReadLaneAt(oldValue, 1);
  RepackingPoint(6);
  p.value += WaveReadLaneAt(oldValue, 1);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 7
  // CHECK: call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 7
  // CHECK: call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32
  RepackingPoint(7);
  p.value += WaveReadLaneFirst(oldValue);
  RepackingPoint(7);
  p.value += WaveReadLaneFirst(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 8
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 8
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  RepackingPoint(8);
  p.value += WaveActiveSum(oldValue);
  RepackingPoint(8);
  p.value += WaveActiveSum(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 9
  // CHECK: call i64 @dx.op.waveActiveOp.i64(i32 119, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 9
  // CHECK: call i64 @dx.op.waveActiveOp.i64(i32 119, i64
  RepackingPoint(9);
  p.value += WaveActiveProduct(oldValue == 17 ? 1 : 0);
  RepackingPoint(9);
  p.value += WaveActiveProduct(oldValue == 17 ? 1 : 0);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 10
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 10
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  RepackingPoint(10);
  p.value += WaveActiveBitAnd(oldValue);
  RepackingPoint(10);
  p.value += WaveActiveBitAnd(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 11
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 11
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  RepackingPoint(11);
  p.value += WaveActiveBitXor(oldValue);
  RepackingPoint(11);
  p.value += WaveActiveBitXor(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 12
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 12
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  RepackingPoint(12);
  p.value += WaveActiveMin(oldValue);
  RepackingPoint(12);
  p.value += WaveActiveMin(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 13
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 13
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  RepackingPoint(13);
  p.value += WaveActiveMax(oldValue);
  RepackingPoint(13);
  p.value += WaveActiveMax(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 14
  // CHECK: call i32 @dx.op.wavePrefixOp.i32(i32 121, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 14
  // CHECK: call i32 @dx.op.wavePrefixOp.i32(i32 121, i32
  RepackingPoint(14);
  p.value += WavePrefixSum(oldValue);
  RepackingPoint(14);
  p.value += WavePrefixSum(oldValue);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 15
  // CHECK: call i64 @dx.op.wavePrefixOp.i64(i32 121, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 15
  // CHECK: call i64 @dx.op.wavePrefixOp.i64(i32 121, i64
  RepackingPoint(15);
  p.value += WavePrefixProduct(oldValue == 17 ? 1 : 0);
  RepackingPoint(15);
  p.value += WavePrefixProduct(oldValue == 17 ? 1 : 0);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 16
  // CHECK: call i32 @dx.op.waveAllOp(i32 135, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 16
  // CHECK: call i32 @dx.op.waveAllOp(i32 135, i1
  RepackingPoint(16);
  p.value += WaveActiveCountBits(oldValue == 17);
  RepackingPoint(16);
  p.value += WaveActiveCountBits(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 17
  // CHECK: call i32 @dx.op.wavePrefixOp(i32 136, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 17
  // CHECK: call i32 @dx.op.wavePrefixOp(i32 136, i1
  RepackingPoint(17);
  p.value += WavePrefixCountBits(oldValue == 17);
  RepackingPoint(17);
  p.value += WavePrefixCountBits(oldValue == 17);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 18
  // CHECK: call %dx.types.fouri32 @dx.op.waveMatch.i32(i32 165, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 18
  // CHECK: call %dx.types.fouri32 @dx.op.waveMatch.i32(i32 165, i32
  RepackingPoint(18);
  uint4 mask = WaveMatch(oldValue);
  RepackingPoint(18);
  p.value += WaveMatch(oldValue).x;

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 19
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 19
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  RepackingPoint(19);
  p.value += WaveMultiPrefixBitAnd(oldValue, mask);
  RepackingPoint(19);
  p.value += WaveMultiPrefixBitAnd(oldValue, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 20
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 20
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  RepackingPoint(20);
  p.value += WaveMultiPrefixBitOr(oldValue, mask);
  RepackingPoint(20);
  p.value += WaveMultiPrefixBitOr(oldValue, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 21
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 21
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  RepackingPoint(21);
  p.value += WaveMultiPrefixBitXor(oldValue, mask);
  RepackingPoint(21);
  p.value += WaveMultiPrefixBitXor(oldValue, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 22
  // CHECK: call i32 @dx.op.waveMultiPrefixBitCount(i32 167, i1
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 22
  // CHECK: call i32 @dx.op.waveMultiPrefixBitCount(i32 167, i1
  RepackingPoint(22);
  p.value += WaveMultiPrefixCountBits(oldValue == 17, mask);
  RepackingPoint(22);
  p.value += WaveMultiPrefixCountBits(oldValue == 17, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 23
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 23
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  RepackingPoint(23);
  p.value += WaveMultiPrefixProduct(oldValue == 17 ? 1 : 0, mask);
  RepackingPoint(23);
  p.value += WaveMultiPrefixProduct(oldValue == 17 ? 1 : 0, mask);

  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 24
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  // CHECK: call void @dx.op.callShader.struct.Payload(i32 159, i32 24
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  RepackingPoint(24);
  p.value += WaveMultiPrefixSum(oldValue == 17 ? 1 : 0, mask);
  RepackingPoint(24);
  p.value += WaveMultiPrefixSum(oldValue == 17 ? 1 : 0, mask);
}
