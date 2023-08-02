// RUN: %dxc -T cs_6_0 -E main

groupshared int64_t dest_i;
groupshared uint64_t dest_u;

RWStructuredBuffer<uint64_t> buff;

RWStructuredBuffer<uint64_t> getDest() {
  return buff;
}

[numthreads(1,1,1)]
void main()
{
  uint64_t original_u_val;
  int64_t original_i_val;

  int64_t   val1_i64;
  int64_t   val2_i64;
  uint64_t  val3_u64;

  ////////////////////////////////////////////////////////
  ///////      Test all Interlocked* functions      //////
  ////////////////////////////////////////////////////////

// CHECK: OpCapability Int64
// CHECK: OpCapability Int64Atomics

// CHECK:        [[val1_i64:%\d+]] = OpLoad %long %val1_i64
// CHECK-NEXT: [[atomic_add:%\d+]] = OpAtomicIAdd %long %dest_i %uint_1 %uint_0 [[val1_i64]]
// CHECK-NEXT:                       OpStore %original_i_val [[atomic_add]]
  InterlockedAdd(dest_i, val1_i64, original_i_val);

// CHECK:      [[fn_call_result:%\d+]] = OpFunctionCall %_ptr_Uniform_type_RWStructuredBuffer_uint64 %getDest
// CHECK-NEXT:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_ulong [[fn_call_result]] %int_0 %uint_0
// CHECK-NEXT:       [[val3_u64:%\d+]] = OpLoad %ulong %val3_u64
// CHECK-NEXT:     [[atomic_add:%\d+]] = OpAtomicIAdd %ulong [[ptr]] %uint_1 %uint_0 [[val3_u64]]
// CHECK-NEXT:                           OpStore %original_u_val [[atomic_add]]
  InterlockedAdd(getDest()[0], val3_u64, original_u_val);

// CHECK:        [[val3_u64:%\d+]] = OpLoad %ulong %val3_u64
// CHECK-NEXT: [[atomic_and:%\d+]] = OpAtomicAnd %ulong %dest_u %uint_1 %uint_0 [[val3_u64]]
// CHECK-NEXT:                       OpStore %original_u_val [[atomic_and]]
  InterlockedAnd(dest_u, val3_u64,  original_u_val);

// CHECK:        [[val1_i64:%\d+]] = OpLoad %long %val1_i64
// CHECK-NEXT: [[atomic_max:%\d+]] = OpAtomicSMax %long %dest_i %uint_1 %uint_0 [[val1_i64]]
// CHECK-NEXT:                       OpStore %original_i_val [[atomic_max]]
  InterlockedMax(dest_i, val1_i64,  original_i_val);

// CHECK:        [[val3_u64:%\d+]] = OpLoad %ulong %val3_u64
// CHECK-NEXT: [[atomic_min:%\d+]] = OpAtomicUMin %ulong %dest_u %uint_1 %uint_0 [[val3_u64]]
// CHECK-NEXT:                       OpStore %original_u_val [[atomic_min]]
  InterlockedMin(dest_u, val3_u64,  original_u_val);

// CHECK:       [[val2_i64:%\d+]] = OpLoad %long %val2_i64
// CHECK-NEXT: [[atomic_or:%\d+]] = OpAtomicOr %long %dest_i %uint_1 %uint_0 [[val2_i64:%\d+]]
// CHECK-NEXT:                      OpStore %original_i_val [[atomic_or]]
  InterlockedOr (dest_i, val2_i64, original_i_val);

// CHECK:        [[val3_u64:%\d+]] = OpLoad %ulong %val3_u64
// CHECK-NEXT: [[atomic_xor:%\d+]] = OpAtomicXor %ulong %dest_u %uint_1 %uint_0 [[val3_u64]]
// CHECK-NEXT:                       OpStore %original_u_val [[atomic_xor]]
  InterlockedXor(dest_u, val3_u64,  original_u_val);

// CHECK:      [[val1_i64:%\d+]] = OpLoad %long %val1_i64
// CHECK-NEXT: [[val2_i64:%\d+]] = OpLoad %long %val2_i64
// CHECK-NEXT:          {{%\d+}} = OpAtomicCompareExchange %long %dest_i %uint_1 %uint_0 %uint_0 [[val2_i64]] [[val1_i64]]
  InterlockedCompareStore(dest_i, val1_i64, val2_i64);

// CHECK:      [[ace:%\d+]] = OpAtomicCompareExchange %ulong %dest_u %uint_1 %uint_0 %uint_0 %ulong_20 %ulong_15
// CHECK-NEXT:                OpStore %original_u_val [[ace]]
  InterlockedCompareExchange(dest_u, 15u, 20u, original_u_val);

// CHECK:      [[val2_i64:%\d+]] = OpLoad %long %val2_i64
// CHECK-NEXT:       [[ae:%\d+]] = OpAtomicExchange %long %dest_i %uint_1 %uint_0 [[val2_i64]]
// CHECK-NEXT:                     OpStore %original_i_val [[ae]]
  InterlockedExchange(dest_i, val2_i64, original_i_val);
}

