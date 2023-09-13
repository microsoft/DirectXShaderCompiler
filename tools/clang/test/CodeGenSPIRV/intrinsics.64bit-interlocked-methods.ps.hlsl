// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability Int64
// CHECK: OpCapability Int64Atomics

// 64-bit atomics were introduced in SM 6.6.
RWStructuredBuffer<uint64_t> rwb_u64;
RWStructuredBuffer<int64_t> rwb_i64;

void main() {
  uint64_t out_u64;
  uint64_t in_u64;

  int64_t out_i64;
  int64_t in_i64;

  uint  index;

// CHECK:           [[index:%\d+]] = OpLoad %uint %index
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_ulong %rwb_u64 %int_0 [[index]]
// CHECK-NEXT:     [[in_u64:%\d+]] = OpLoad %ulong %in_u64
// CHECK-NEXT: [[atomic_add:%\d+]] = OpAtomicIAdd %ulong [[ptr]] %uint_1 %uint_0 [[in_u64]]
// CHECK-NEXT:                       OpStore %out_u64 [[atomic_add]]
  InterlockedAdd(rwb_u64[index], in_u64, out_u64);

// CHECK:           [[index:%\d+]] = OpLoad %uint %index
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_long %rwb_i64 %int_0 [[index]]
// CHECK-NEXT:     [[in_i64:%\d+]] = OpLoad %long %in_i64
// CHECK-NEXT: [[atomic_add:%\d+]] = OpAtomicIAdd %long [[ptr]] %uint_1 %uint_0 [[in_i64]]
// CHECK-NEXT:                       OpStore %out_i64 [[atomic_add]]
  InterlockedAdd(rwb_i64[index], in_i64, out_i64);
}

