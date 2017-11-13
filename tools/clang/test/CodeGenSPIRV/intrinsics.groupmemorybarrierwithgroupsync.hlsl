// Run: %dxc -T cs_6_0 -E main

void main() {
// CHECK: OpControlBarrier %uint_2 %uint_2 %uint_256
  GroupMemoryBarrierWithGroupSync();
}
