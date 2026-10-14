// RUN: %dxc -T cs_6_6 -E main %s | FileCheck %s

// BDCE must drop nsw/nuw/exact from users of a value it replaces with zero.
//
// InstCombine marks 'sub1' as 'nuw' based on bit 15 of 'x'. BDCE replaces 'x'
// with 0 because only the low bits of its shifts are used. Keeping the 'nuw'
// made 'sub1' poison, which InstCombine folded to 0, so 'idx' (always 0 at
// run time) became the out-of-range constant 98302 (GEP indices 6143, 14).

// CHECK: define void @main()
// CHECK-NOT: i32 6143, i32 14
// CHECK: getelementptr [16 x i32], [16 x i32] addrspace(3)* @{{.*}}, i32 0, i32 %[[IDX:[0-9]+]]
// CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 0, i32 undef, i32 %[[IDX]],

RWByteAddressBuffer out_buf : register(u0);
groupshared uint wg_arr[16];

[numthreads(2, 1, 1)]
void main(uint lid : SV_GroupIndex) {
  uint shr = lid >> 2u;
  uint x = (((shr << 31u) >> 17u) + (shr << 16u)) + 32768u;
  uint rhs = (shr << 1u) + 1u;
  uint sub1 = (x << 16u) - rhs;
  uint sub2 = (x << 15u) - rhs;
  uint idx = 98302u - ((sub1 % 65536u) + (sub1 % 32768u));
  wg_arr[idx] = 1u;
  out_buf.Store(0u, idx);
  out_buf.Store(4u, (sub2 % 32768u) + (sub2 % 16384u));
}
