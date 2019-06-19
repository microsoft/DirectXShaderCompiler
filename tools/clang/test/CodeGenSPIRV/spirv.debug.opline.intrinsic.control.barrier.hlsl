// Run: %dxc -T cs_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.control.barrier.hlsl

groupshared int dest_i;

// Note that preprocessor prepends a "#line 1 ..." line to the whole file and
// the compliation sees line numbers incremented by 1.

void main() {

// CHECK:      OpLine [[file]] 16 3
// CHECK-NEXT: OpControlBarrier %uint_2 %uint_1 %uint_2376
  AllMemoryBarrierWithGroupSync();

// CHECK-NEXT: OpLine [[file]] 20 3
// CHECK-NEXT: OpMemoryBarrier %uint_1 %uint_2120
  DeviceMemoryBarrier();

// CHECK-NEXT: OpLine [[file]] 24 3
// CHECK-NEXT: OpMemoryBarrier %uint_2 %uint_264
  GroupMemoryBarrier();
}
