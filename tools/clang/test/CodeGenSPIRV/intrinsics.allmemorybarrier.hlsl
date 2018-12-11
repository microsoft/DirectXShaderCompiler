// Run: %dxc -T cs_6_0 -E main

// Memory scope : Device = 0x1 = 1
// Semantics: ImageMemory | UniformMemory | WorkgroupMemory | AcquireRelease = 0x800 | 0x40 | 0x100 | 0x8 = 2376

void main() {
// CHECK: OpMemoryBarrier %uint_1 %uint_2376
  AllMemoryBarrier();
}
