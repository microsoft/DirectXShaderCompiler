// Run: %dxc -T cs_6_0 -E main -Zi -fspv-target-env=vulkan1.1

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.vulkan1.1.hlsl

// Note that preprocessor prepends a "#line 1 ..." line to the whole file and
// the compliation sees line numbers incremented by 1.
void main() {
// CHECK:      OpLine [[file]] 14 11
// CHECK:      OpLoad %uint %SubgroupSize
// CHECK-NEXT: OpLine [[file]] 14 32
// CHECK-NEXT: OpLoad %uint %SubgroupLocalInvocationId
  int i = WaveGetLaneCount() + WaveGetLaneIndex();

// CHECK:      OpLine [[file]] 18 3
// CHECK-NEXT: OpGroupNonUniformElect %bool %uint_3
  WaveIsFirstLane();

// CHECK:      OpLine [[file]] 22 3
// CHECK-NEXT: OpGroupNonUniformAll %bool %uint_3
  WaveActiveAllTrue(i == 1);

// CHECK:      OpLine [[file]] 26 3
// CHECK-NEXT: OpGroupNonUniformAny %bool %uint_3
  WaveActiveAnyTrue(i == 0);

// CHECK:      OpLine [[file]] 30 3
// CHECK-NEXT: OpGroupNonUniformBallot %v4uint %uint_3
  WaveActiveBallot(i == 2);

// CHECK:      OpLine [[file]] 34 3
// CHECK-NEXT: OpGroupNonUniformAllEqual %bool %uint_3
  WaveActiveAllEqual(i);

// CHECK:      OpLine [[file]] 39 3
// CHECK-NEXT: OpGroupNonUniformBallot %v4uint %uint_3
// CHECK-NEXT: OpGroupNonUniformBallotBitCount %uint %uint_3 Reduce
  WaveActiveCountBits(i);

// CHECK:      OpLine [[file]] 43 3
// CHECK-NEXT: OpGroupNonUniformIAdd %int %uint_3 Reduce
  WaveActiveSum(i);

// CHECK:      OpLine [[file]] 47 3
// CHECK-NEXT: OpGroupNonUniformIAdd %int %uint_3 ExclusiveScan
  WavePrefixSum(i);

// CHECK:      OpLine [[file]] 52 3
// CHECK-NEXT: OpGroupNonUniformBallot %v4uint %uint_3
// CHECK-NEXT: OpGroupNonUniformBallotBitCount %uint %uint_3 ExclusiveScan
  WavePrefixCountBits(i == 1);

// CHECK:      OpLine [[file]] 56 3
// CHECK-NEXT: OpGroupNonUniformBroadcast %int %uint_3
  WaveReadLaneAt(i, 15);

// CHECK:      OpLine [[file]] 60 3
// CHECK-NEXT: OpGroupNonUniformQuadBroadcast %int %uint_3
  QuadReadLaneAt(i, 15);
}
