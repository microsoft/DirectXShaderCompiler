// RUN: %dxc -T cs_6_6 -E main %s | FileCheck %s

// Regression test for a bug where the optimizer (JumpThreading) would
// restructure a while-loop containing wave intrinsics, moving
// WaveActiveCountBits outside the loop. This changes the set of active
// lanes at the wave op call site, producing incorrect results on SIMT
// hardware.

// Verify that WaveAllBitCount (opcode 135) appears BEFORE the loop's
// back-edge phi, ensuring it stays inside the loop body.

// CHECK: call i32 @dx.op.waveReadLaneFirst
// CHECK: call i32 @dx.op.waveAllOp
// CHECK: call i1 @dx.op.waveIsFirstLane
// CHECK: phi i32
// CHECK: br i1

RWStructuredBuffer<uint> Output : register(u1);

cbuffer Constants : register(b0) {
  uint Width;
  uint Height;
  uint NumMaterials;
};

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
  uint x = DTid.x;
  uint y = DTid.y;

  if (x >= Width || y >= Height)
    return;

  // Compute a material ID per lane (simple hash).
  uint materialID = ((x * 7) + (y * 13)) % NumMaterials;

  // Binning loop: each iteration peels off one material group.
  // WaveReadLaneFirst picks a material, matching lanes count themselves
  // with WaveActiveCountBits, and the first lane in the group writes
  // the count. Non-matching lanes loop back for the next material.
  bool go = true;
  while (go) {
    uint firstMat = WaveReadLaneFirst(materialID);
    if (firstMat == materialID) {
      uint count = WaveActiveCountBits(true);
      if (WaveIsFirstLane()) {
        InterlockedAdd(Output[firstMat], count);
      }
      go = false;
    }
  }
}
