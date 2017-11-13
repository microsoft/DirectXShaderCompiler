// Run: %dxc -T cs_6_0 -E main

void main() {
// CHECK: OpControlBarrier %uint_1 %uint_1 %uint_4048
  AllMemoryBarrierWithGroupSync();
}
