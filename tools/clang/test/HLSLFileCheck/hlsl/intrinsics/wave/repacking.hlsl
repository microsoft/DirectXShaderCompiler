// RUN: %dxc -DUSE_CALLSHADER=0 -T lib_6_5 %s | FileCheck %s
// RUN: %dxc -DUSE_CALLSHADER=1 -T lib_6_5 %s | FileCheck %s

// Check that results of wave intrinsics are not re-used
// cross DXR repacking points such as TraceRay() or CallShader();
struct Payload {
  unsigned int value;
};

RaytracingAccelerationStructure myAccelerationStructure : register(t3);

void RepackingPoint(int identifier) {
  Payload p;
#if USE_CALLSHADER
  CallShader(identifier, p);
#else
  RayDesc myRay = { float3(0., 0., 0.), 0., float3(0., 0., 0.), 1.0};
  TraceRay(myAccelerationStructure, 0, -1, 0, 0, identifier, myRay, p);
#endif
}

[shader("miss")] void Miss(inout Payload p) {
  // Some runtime value that we can't reason about.
  // Freeze the current value and use it as input to
  // all intrinsics so that we know the intrinsics' result
  // would be the same before and after the CallShader call,
  // except for repacking.
  unsigned int oldValue = p.value;

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 0
  // CHECK: call i1 @dx.op.waveIsFirstLane(i32 110
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 1
  // CHECK: call i1 @dx.op.waveIsFirstLane(i32 110
  RepackingPoint(0);
  p.value += WaveIsFirstLane();
  RepackingPoint(1);
  p.value += WaveIsFirstLane();

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 2
  // CHECK: call i32 @dx.op.waveGetLaneIndex(i32 111
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 3
  // CHECK: call i32 @dx.op.waveGetLaneIndex(i32 111
  RepackingPoint(2);
  p.value += WaveGetLaneIndex();
  RepackingPoint(3);
  p.value += WaveGetLaneIndex();

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 4
  // CHECK: call i1 @dx.op.waveAnyTrue(i32 113, i1
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 5
  // CHECK: call i1 @dx.op.waveAnyTrue(i32 113, i1
  RepackingPoint(4);
  p.value += WaveActiveAnyTrue(oldValue == 17);
  RepackingPoint(5);
  p.value += WaveActiveAnyTrue(oldValue == 17);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 6
  // CHECK: call i1 @dx.op.waveAllTrue(i32 114, i1
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 7
  // CHECK: call i1 @dx.op.waveAllTrue(i32 114, i1
  RepackingPoint(6);
  p.value += WaveActiveAllTrue(oldValue == 17);
  RepackingPoint(7);
  p.value += WaveActiveAllTrue(oldValue == 17);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 8
  // CHECK: call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 9
  // CHECK: call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32
  RepackingPoint(8);
  p.value += WaveActiveAllEqual(oldValue);
  RepackingPoint(9);
  p.value += WaveActiveAllEqual(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 10
  // CHECK: call %dx.types.fouri32 @dx.op.waveActiveBallot(i32 116, i1
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 11
  // CHECK: call %dx.types.fouri32 @dx.op.waveActiveBallot(i32 116, i1
  RepackingPoint(10);
  p.value += WaveActiveBallot(oldValue).x;
  RepackingPoint(11);
  p.value += WaveActiveBallot(oldValue).x;

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 12
  // CHECK: call i32 @dx.op.waveReadLaneAt.i32(i32 117, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 13
  // CHECK: call i32 @dx.op.waveReadLaneAt.i32(i32 117, i32
  RepackingPoint(12);
  p.value += WaveReadLaneAt(oldValue, 1);
  RepackingPoint(13);
  p.value += WaveReadLaneAt(oldValue, 1);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 14
  // CHECK: call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 15
  // CHECK: call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32
  RepackingPoint(14);
  p.value += WaveReadLaneFirst(oldValue);
  RepackingPoint(15);
  p.value += WaveReadLaneFirst(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 16
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 17
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  RepackingPoint(16);
  p.value += WaveActiveSum(oldValue);
  RepackingPoint(17);
  p.value += WaveActiveSum(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 18
  // CHECK: call i64 @dx.op.waveActiveOp.i64(i32 119, i64
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 19
  // CHECK: call i64 @dx.op.waveActiveOp.i64(i32 119, i64
  RepackingPoint(18);
  p.value += WaveActiveProduct(oldValue == 17 ? 1 : 0);
  RepackingPoint(19);
  p.value += WaveActiveProduct(oldValue == 17 ? 1 : 0);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 20
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 21
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  RepackingPoint(20);
  p.value += WaveActiveBitAnd(oldValue);
  RepackingPoint(21);
  p.value += WaveActiveBitAnd(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 22
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 23
  // CHECK: call i32 @dx.op.waveActiveBit.i32(i32 120, i32
  RepackingPoint(22);
  p.value += WaveActiveBitXor(oldValue);
  RepackingPoint(23);
  p.value += WaveActiveBitXor(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 24
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 25
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  RepackingPoint(24);
  p.value += WaveActiveMin(oldValue);
  RepackingPoint(25);
  p.value += WaveActiveMin(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 26
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 27
  // CHECK: call i32 @dx.op.waveActiveOp.i32(i32 119, i32
  RepackingPoint(26);
  p.value += WaveActiveMax(oldValue);
  RepackingPoint(27);
  p.value += WaveActiveMax(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 28
  // CHECK: call i32 @dx.op.wavePrefixOp.i32(i32 121, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 29
  // CHECK: call i32 @dx.op.wavePrefixOp.i32(i32 121, i32
  RepackingPoint(28);
  p.value += WavePrefixSum(oldValue);
  RepackingPoint(29);
  p.value += WavePrefixSum(oldValue);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 30
  // CHECK: call i64 @dx.op.wavePrefixOp.i64(i32 121, i64
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 31
  // CHECK: call i64 @dx.op.wavePrefixOp.i64(i32 121, i64
  RepackingPoint(30);
  p.value += WavePrefixProduct(oldValue == 17 ? 1 : 0);
  RepackingPoint(31);
  p.value += WavePrefixProduct(oldValue == 17 ? 1 : 0);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 32
  // CHECK: call i32 @dx.op.waveAllOp(i32 135, i1
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 33
  // CHECK: call i32 @dx.op.waveAllOp(i32 135, i1
  RepackingPoint(32);
  p.value += WaveActiveCountBits(oldValue == 17);
  RepackingPoint(33);
  p.value += WaveActiveCountBits(oldValue == 17);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 34
  // CHECK: call i32 @dx.op.wavePrefixOp(i32 136, i1
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 35
  // CHECK: call i32 @dx.op.wavePrefixOp(i32 136, i1
  RepackingPoint(34);
  p.value += WavePrefixCountBits(oldValue == 17);
  RepackingPoint(35);
  p.value += WavePrefixCountBits(oldValue == 17);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 36
  // CHECK: call %dx.types.fouri32 @dx.op.waveMatch.i32(i32 165, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 37
  // CHECK: call %dx.types.fouri32 @dx.op.waveMatch.i32(i32 165, i32
  RepackingPoint(36);
  uint4 mask = WaveMatch(oldValue);
  RepackingPoint(37);
  p.value += WaveMatch(oldValue).x;

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 38
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 39
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  RepackingPoint(38);
  p.value += WaveMultiPrefixBitAnd(oldValue, mask);
  RepackingPoint(39);
  p.value += WaveMultiPrefixBitAnd(oldValue, mask);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 40
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 41
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  RepackingPoint(40);
  p.value += WaveMultiPrefixBitOr(oldValue, mask);
  RepackingPoint(41);
  p.value += WaveMultiPrefixBitOr(oldValue, mask);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 42
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 43
  // CHECK: call i32 @dx.op.waveMultiPrefixOp.i32(i32 166, i32
  RepackingPoint(42);
  p.value += WaveMultiPrefixBitXor(oldValue, mask);
  RepackingPoint(43);
  p.value += WaveMultiPrefixBitXor(oldValue, mask);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 44
  // CHECK: call i32 @dx.op.waveMultiPrefixBitCount(i32 167, i1
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 45
  // CHECK: call i32 @dx.op.waveMultiPrefixBitCount(i32 167, i1
  RepackingPoint(44);
  p.value += WaveMultiPrefixCountBits(oldValue == 17, mask);
  RepackingPoint(45);
  p.value += WaveMultiPrefixCountBits(oldValue == 17, mask);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 46
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 47
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  RepackingPoint(46);
  p.value += WaveMultiPrefixProduct(oldValue == 17 ? 1 : 0, mask);
  RepackingPoint(47);
  p.value += WaveMultiPrefixProduct(oldValue == 17 ? 1 : 0, mask);

  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 48
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  // CHECK: call void @dx.op.{{traceRay|callShader}}.struct.Payload({{.*}} i32 49
  // CHECK: call i64 @dx.op.waveMultiPrefixOp.i64(i32 166, i64
  RepackingPoint(48);
  p.value += WaveMultiPrefixSum(oldValue == 17 ? 1 : 0, mask);
  RepackingPoint(49);
  p.value += WaveMultiPrefixSum(oldValue == 17 ? 1 : 0, mask);
}
