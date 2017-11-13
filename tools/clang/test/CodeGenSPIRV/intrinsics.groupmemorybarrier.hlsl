// Run: %dxc -T cs_6_0 -E main

void main() {
// CHECK: OpMemoryBarrier %uint_2 %uint_256
  GroupMemoryBarrier();
}
