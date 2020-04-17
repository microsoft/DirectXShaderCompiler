// Run: %dxc -T cs_6_0 -E main

groupshared int dest_i;
groupshared uint dest_u;

RWBuffer<uint> buff;
RWBuffer<uint> getDest() {
  return buff;
}

[numthreads(1,1,1)]
void main()
{
  uint original_u_val;
  int original_i_val;

  int   val1;
  int   val2;

  //////////////////////////////////////////////////////////////////////////
  ///////      Test all Interlocked* functions on primitive types     //////
  ///////                Only int and uint are allowd                 //////
  //////////////////////////////////////////////////////////////////////////

// CHECK:      [[val1_27:%\d+]] = OpLoad %int %val1
// CHECK-NEXT: [[iadd27:%\d+]] = OpAtomicIAdd %int %dest_i %uint_1 %uint_0 [[val1_27]]
// CHECK-NEXT:                   OpStore %original_i_val [[iadd27]]
  InterlockedAdd(dest_i, val1, original_i_val);

// CHECK:      [[buff:%\d+]] = OpFunctionCall %type_buffer_image %getDest
// CHECK-NEXT: OpStore %temp_var_RWBuffer [[buff]]
// CHECK-NEXT: OpImageTexelPointer %_ptr_Image_uint %temp_var_RWBuffer %uint_0 %uint_0
  InterlockedAdd(getDest()[0], val1, original_i_val);

// CHECK:      [[and28:%\d+]] = OpAtomicAnd %uint %dest_u %uint_1 %uint_0 %uint_10
// CHECK-NEXT:                  OpStore %original_u_val [[and28]]
  InterlockedAnd(dest_u, 10,  original_u_val);

// CHECK:       [[uint10:%\d+]] = OpBitcast %int %uint_10
// CHECK-NEXT: [[asmax29:%\d+]] = OpAtomicSMax %int %dest_i %uint_1 %uint_0 [[uint10]]
// CHECK-NEXT:                    OpStore %original_i_val [[asmax29]]
  InterlockedMax(dest_i, 10,  original_i_val);

// CHECK:      [[umin30:%\d+]] = OpAtomicUMin %uint %dest_u %uint_1 %uint_0 %uint_10
// CHECK-NEXT:                   OpStore %original_u_val [[umin30]]
  InterlockedMin(dest_u, 10,  original_u_val);

// CHECK:      [[val2_31:%\d+]] = OpLoad %int %val2
// CHECK-NEXT:   [[or31:%\d+]] = OpAtomicOr %int %dest_i %uint_1 %uint_0 [[val2_31]]
// CHECK-NEXT:                   OpStore %original_i_val [[or31]]
  InterlockedOr (dest_i, val2, original_i_val);

// CHECK:      [[xor32:%\d+]] = OpAtomicXor %uint %dest_u %uint_1 %uint_0 %uint_10
// CHECK-NEXT:                  OpStore %original_u_val [[xor32]]
  InterlockedXor(dest_u, 10,  original_u_val);

// CHECK:      [[val1_33:%\d+]] = OpLoad %int %val1
// CHECK-NEXT: [[val2_33:%\d+]] = OpLoad %int %val2
// CHECK-NEXT:        {{%\d+}} = OpAtomicCompareExchange %int %dest_i %uint_1 %uint_0 %uint_0 [[val2_33]] [[val1_33]]
  InterlockedCompareStore(dest_i, val1, val2);

// CHECK:      [[ace34:%\d+]] = OpAtomicCompareExchange %uint %dest_u %uint_1 %uint_0 %uint_0 %uint_20 %uint_15
// CHECK-NEXT:                  OpStore %original_u_val [[ace34]]
  InterlockedCompareExchange(dest_u, 15, 20, original_u_val);

// CHECK:      [[val2_35:%\d+]] = OpLoad %int %val2
// CHECK-NEXT:  [[ace35:%\d+]] = OpAtomicExchange %int %dest_i %uint_1 %uint_0 [[val2_35]]
// CHECK-NEXT:                   OpStore %original_i_val [[ace35]]
  InterlockedExchange(dest_i, val2, original_i_val);
}
