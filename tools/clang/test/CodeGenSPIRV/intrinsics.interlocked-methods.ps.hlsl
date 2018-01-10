// Run: %dxc -T ps_6_0 -E main

RWTexture1D <int>   g_tTex1di1;
RWTexture1D <uint>  g_tTex1du1;

RWTexture2D <int>   g_tTex2di1;
RWTexture2D <uint>  g_tTex2du1;

RWTexture3D <int>   g_tTex3di1;
RWTexture3D <uint>  g_tTex3du1;

RWTexture1DArray <int>   g_tTex1di1a;
RWTexture1DArray <uint>  g_tTex1du1a;

RWTexture2DArray <int>   g_tTex2di1a;
RWTexture2DArray <uint>  g_tTex2du1a;

RWBuffer <int>   g_tBuffI;
RWBuffer <uint>  g_tBuffU;

RWStructuredBuffer<uint> g_tRWBuffU;

void main()
{
  uint out_u1;
  int out_i1;

  uint  u1;
  uint2 u2;
  uint3 u3;
  uint  u1b;
  uint  u1c;

  int   i1;
  int2  i2;
  int3  i3;
  int   i1b;
  int   i1c;

  ////////////////////////////////////////////////////////////////////
  /////   Test that type mismatches are resolved correctly    ////////
  ////////////////////////////////////////////////////////////////////

// CHECK:         [[idx0:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:    [[ptr0:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 [[idx0]] %uint_0
// CHECK-NEXT:    [[i1_0:%\d+]] = OpLoad %int %i1
// CHECK-NEXT:   [[iadd0:%\d+]] = OpAtomicIAdd %int [[ptr0]] %uint_1 %uint_0 [[i1_0]]
// CHECK-NEXT: [[iadd0_u:%\d+]] = OpBitcast %uint [[iadd0]]
// CHECK-NEXT:                    OpStore %out_u1 [[iadd0_u]]
  InterlockedAdd(g_tTex1di1[u1], i1, out_u1); // Addition result must be cast to uint before being written to out_u1


// CHECK:        [[ptr1:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT:   [[u1_1:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT: [[u1_int:%\d+]] = OpBitcast %int [[u1_1]]
// CHECK-NEXT:  [[iadd1:%\d+]] = OpAtomicIAdd %int [[ptr1]] %uint_1 %uint_0 [[u1_int]]
// CHECK-NEXT:                   OpStore %out_i1 [[iadd1]]
  InterlockedAdd(g_tTex1di1[u1], u1, out_i1); // u1 should be cast to int before being passed to addition instruction

// CHECK:         [[ptr2:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT:    [[i1_2:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[i1_uint:%\d+]] = OpBitcast %uint [[i1_2]]
// CHECK-NEXT:   [[iadd2:%\d+]] = OpAtomicIAdd %uint [[ptr2]] %uint_1 %uint_0 [[i1_uint]]
// CHECK-NEXT:                    OpStore %out_u1 [[iadd2]]
  InterlockedAdd(g_tTex1du1[u1], i1, out_u1); // i1 should be cast to uint before being passed to addition instruction

// CHECK:           [[ptr3:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT:      [[u1_3:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:     [[iadd3:%\d+]] = OpAtomicIAdd %uint [[ptr3]] %uint_1 %uint_0 [[u1_3]]
// CHECK-NEXT: [[iadd3_int:%\d+]] = OpBitcast %int [[iadd3]]
// CHECK-NEXT:                      OpStore %out_i1 [[iadd3_int]]
  InterlockedAdd(g_tTex1du1[u1], u1, out_i1); // Addition result must be cast to int before being written to out_i1


// CHECK:           [[ptr4:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT:     [[u1b_4:%\d+]] = OpLoad %uint %u1b
// CHECK-NEXT: [[u1b_4_int:%\d+]] = OpBitcast %int [[u1b_4]]
// CHECK-NEXT:     [[i1c_4:%\d+]] = OpLoad %int %i1c
// CHECK-NEXT:      [[ace4:%\d+]] = OpAtomicCompareExchange %int [[ptr4]] %uint_1 %uint_0 %uint_0 [[i1c_4]] [[u1b_4_int]]
// CHECK-NEXT:                      OpStore %out_i1 [[ace4]]
  InterlockedCompareExchange(g_tTex1di1[u1], u1b, i1c, out_i1); // u1b should first be cast to int


// CHECK:           [[ptr5:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT:     [[i1b_5:%\d+]] = OpLoad %int %i1b
// CHECK-NEXT:     [[u1c_5:%\d+]] = OpLoad %uint %u1c
// CHECK-NEXT: [[u1c_5_int:%\d+]] = OpBitcast %int [[u1c_5]]
// CHECK-NEXT:      [[ace5:%\d+]] = OpAtomicCompareExchange %int [[ptr5]] %uint_1 %uint_0 %uint_0 [[u1c_5_int]] [[i1b_5]]
// CHECK-NEXT:                      OpStore %out_i1 [[ace5]]
  InterlockedCompareExchange(g_tTex1di1[u1], i1b, u1c, out_i1); // u1c should first be cast to int

// CHECK:           [[ptr6:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT:     [[i1b_6:%\d+]] = OpLoad %int %i1b
// CHECK-NEXT:     [[i1c_6:%\d+]] = OpLoad %int %i1c
// CHECK-NEXT:      [[ace6:%\d+]] = OpAtomicCompareExchange %int [[ptr6]] %uint_1 %uint_0 %uint_0 [[i1c_6]] [[i1b_6]]
// CHECK-NEXT: [[ace6_uint:%\d+]] = OpBitcast %uint [[ace6]]
// CHECK-NEXT:                      OpStore %out_u1 [[ace6_uint]]
  InterlockedCompareExchange(g_tTex1di1[u1], i1b, i1c, out_u1); // original value must be cast to uint before being written to out_u1

// CHECK:            [[ptr7:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT:      [[u1b_7:%\d+]] = OpLoad %uint %u1b
// CHECK-NEXT:      [[i1c_7:%\d+]] = OpLoad %int %i1c
// CHECK-NEXT: [[i1c_7_uint:%\d+]] = OpBitcast %uint [[i1c_7]]
// CHECK-NEXT:       [[ace7:%\d+]] = OpAtomicCompareExchange %uint [[ptr7]] %uint_1 %uint_0 %uint_0 [[i1c_7_uint]] [[u1b_7]]
// CHECK-NEXT:                       OpStore %out_u1 [[ace7]]
  InterlockedCompareExchange(g_tTex1du1[u1], u1b, i1c, out_u1); // i1c should first be cast to uint


// CHECK:            [[ptr8:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT:      [[i1b_8:%\d+]] = OpLoad %int %i1b
// CHECK-NEXT: [[i1b_8_uint:%\d+]] = OpBitcast %uint [[i1b_8]]
// CHECK-NEXT:      [[u1c_8:%\d+]] = OpLoad %uint %u1c
// CHECK-NEXT:       [[ace8:%\d+]] = OpAtomicCompareExchange %uint [[ptr8]] %uint_1 %uint_0 %uint_0 [[u1c_8]] [[i1b_8_uint]]
// CHECK-NEXT:                       OpStore %out_u1 [[ace8]]
  InterlockedCompareExchange(g_tTex1du1[u1], i1b, u1c, out_u1); // i1b should first be cast to uint


// CHECK:          [[ptr9:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT:    [[u1b_9:%\d+]] = OpLoad %uint %u1b
// CHECK-NEXT:    [[u1c_9:%\d+]] = OpLoad %uint %u1c
// CHECK-NEXT:     [[ace9:%\d+]] = OpAtomicCompareExchange %uint [[ptr9]] %uint_1 %uint_0 %uint_0 [[u1c_9]] [[u1b_9]]
// CHECK-NEXT: [[ace9_int:%\d+]] = OpBitcast %int [[ace9]]
// CHECK-NEXT:                     OpStore %out_i1 [[ace9_int]]
  InterlockedCompareExchange(g_tTex1du1[u1], u1b, u1c, out_i1); // original value must be cast to int before being written to out_i1


//CHECK:             [[ptr10:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
//CHECK-NEXT:        [[u1_10:%\d+]] = OpLoad %uint %u1
//CHECK-NEXT:    [[u1_10_int:%\d+]] = OpBitcast %int [[u1_10]]
//CHECK-NEXT:      [[asmax10:%\d+]] = OpAtomicSMax %int [[ptr10]] %uint_1 %uint_0 [[u1_10_int]]
//CHECK-NEXT: [[asmax10_uint:%\d+]] = OpBitcast %uint [[asmax10]]
//CHECK-NEXT:                         OpStore %out_u1 [[asmax10_uint]]
  // u1 should be cast to int first.
  // AtomicSMax should be performed.
  // Result should be cast to uint before being written to out_u1.
  InterlockedMax(g_tTex1di1[u1], u1, out_u1);


// CHECK:      [[ptr11:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT: [[i1_11:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[i1_11_uint:%\d+]] = OpBitcast %uint [[i1_11]]
// CHECK-NEXT: [[aumin11:%\d+]] = OpAtomicUMin %uint [[ptr11]] %uint_1 %uint_0 [[i1_11_uint]]
// CHECK-NEXT: [[aumin11_int:%\d+]] = OpBitcast %int [[aumin11]]
// CHECK-NEXT: OpStore %out_i1 [[aumin11_int]]
  // i1 should be cast to uint first.
  // AtomicUMin should be performed.
  // Result should be cast to int before being written to out_i1.
  InterlockedMin(g_tTex1du1[u1], i1, out_i1);



  /////////////////////////////////////////////////////////////////////////////
  /////    Test all Interlocked* functions on various resource types   ////////
  /////////////////////////////////////////////////////////////////////////////

// CHECK:      [[ptr12:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT: [[i1_12:%\d+]] = OpLoad %int %i1
// CHECK-NEXT:       {{%\d+}} = OpAtomicIAdd %int [[ptr12]] %uint_1 %uint_0 [[i1_12]]
  InterlockedAdd            (g_tTex1di1[u1], i1);

// CHECK:       [[ptr13:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT:  [[i1_13:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[iadd13:%\d+]] = OpAtomicIAdd %int [[ptr13]] %uint_1 %uint_0 [[i1_13]]
// CHECK-NEXT:                   OpStore %out_i1 [[iadd13]]
  InterlockedAdd            (g_tTex1di1[u1], i1, out_i1);

// CHECK:      [[ptr14:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT: [[i1_14:%\d+]] = OpLoad %int %i1
// CHECK-NEXT:       {{%\d+}} = OpAtomicAnd %int [[ptr14]] %uint_1 %uint_0 [[i1_14]]
  InterlockedAnd            (g_tTex1di1[u1], i1);

// CHECK:      [[ptr15:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1 {{%\d+}} %uint_0
// CHECK-NEXT: [[i1_15:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[and15:%\d+]] = OpAtomicAnd %int [[ptr15]] %uint_1 %uint_0 [[i1_15]]
// CHECK-NEXT:                  OpStore %out_i1 [[and15]]
  InterlockedAnd            (g_tTex1di1[u1], i1, out_i1);

// CHECK:      [[ptr16:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1 {{%\d+}} %uint_0
// CHECK-NEXT: [[u1_16:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT: {{%\d+}} = OpAtomicUMax %uint [[ptr16]] %uint_1 %uint_0 [[u1_16]]
  InterlockedMax(g_tTex1du1[u1], u1);

// CHECK:        [[u2_17:%\d+]] = OpLoad %v2uint %u2
// CHECK-NEXT:   [[ptr17:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex2di1 [[u2_17]] %uint_0
// CHECK-NEXT:   [[i1_17:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[asmax17:%\d+]] = OpAtomicSMax %int [[ptr17]] %uint_1 %uint_0 [[i1_17]]
// CHECK-NEXT:                    OpStore %out_i1 [[asmax17]]
  InterlockedMax(g_tTex2di1[u2], i1, out_i1);

// CHECK:      [[ptr18:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex2du1 {{%\d+}} %uint_0
// CHECK-NEXT: [[u1_18:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:       {{%\d+}} = OpAtomicUMin %uint [[ptr18]] %uint_1 %uint_0 [[u1_18]]
  InterlockedMin(g_tTex2du1[u2], u1);

// CHECK:        [[u3_19:%\d+]] = OpLoad %v3uint %u3
// CHECK-NEXT:   [[ptr19:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex3di1 [[u3_19]] %uint_0
// CHECK-NEXT:   [[i1_19:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[asmin19:%\d+]] = OpAtomicSMin %int [[ptr19]] %uint_1 %uint_0 [[i1_19]]
// CHECK-NEXT:                    OpStore %out_i1 [[asmin19]]
  InterlockedMin(g_tTex3di1[u3], i1, out_i1);

// CHECK:      [[ptr20:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex3du1 {{%\d+}} %uint_0
// CHECK-NEXT: [[u1_20:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:       {{%\d+}} = OpAtomicOr %uint [[ptr20]] %uint_1 %uint_0 [[u1_20]]
  InterlockedOr (g_tTex3du1[u3], u1);

// CHECK:      [[ptr21:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1a {{%\d+}} %uint_0
// CHECK-NEXT: [[i1_21:%\d+]] = OpLoad %int %i1
// CHECK-NEXT:  [[or21:%\d+]] = OpAtomicOr %int [[ptr21]] %uint_1 %uint_0 [[i1_21]]
// CHECK-NEXT:                  OpStore %out_i1 [[or21]]
  InterlockedOr (g_tTex1di1a[u2], i1, out_i1);

// CHECK:      [[ptr22:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1a {{%\d+}} %uint_0
// CHECK-NEXT: [[u1_22:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:       {{%\d+}} = OpAtomicXor %uint [[ptr22]] %uint_1 %uint_0 [[u1_22]]
  InterlockedXor(g_tTex1du1a[u2], u1);

// CHECK:      [[ptr23:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tTex1di1a {{%\d+}} %uint_0
// CHECK-NEXT: [[i1_23:%\d+]] = OpLoad %int %i1
// CHECK-NEXT: [[xor23:%\d+]] = OpAtomicXor %int [[ptr23]] %uint_1 %uint_0 [[i1_23]]
// CHECK-NEXT:                  OpStore %out_i1 [[xor23]]
  InterlockedXor(g_tTex1di1a[u2], i1, out_i1);

// CHECK:       [[ptr24:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tTex1du1a {{%\d+}} %uint_0
// CHECK-NEXT:  [[u1_24:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT: [[u1b_24:%\d+]] = OpLoad %uint %u1b
// CHECK-NEXT:        {{%\d+}} = OpAtomicCompareExchange %uint [[ptr24]] %uint_1 %uint_0 %uint_0 [[u1b_24]] [[u1_24]]
  InterlockedCompareStore(g_tTex1du1a[u2], u1, u1b);

// CHECK:       [[ptr25:%\d+]] = OpImageTexelPointer %_ptr_Image_int %g_tBuffI {{%\d+}} %uint_0
// CHECK-NEXT: [[i1b_25:%\d+]] = OpLoad %int %i1b
// CHECK-NEXT: [[i1c_25:%\d+]] = OpLoad %int %i1c
// CHECK-NEXT:  [[ace25:%\d+]] = OpAtomicCompareExchange %int [[ptr25]] %uint_1 %uint_0 %uint_0 [[i1c_25]] [[i1b_25]]
// CHECK-NEXT:                   OpStore %out_i1 [[ace25]]
  InterlockedCompareExchange(g_tBuffI[u1], i1b, i1c, out_i1);

// CHECK:      [[ptr26:%\d+]] = OpImageTexelPointer %_ptr_Image_uint %g_tBuffU {{%\d+}} %uint_0
// CHECK-NEXT: [[u1_26:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:  [[ae26:%\d+]] = OpAtomicExchange %uint [[ptr26]] %uint_1 %uint_0 [[u1_26]]
// CHECK-NEXT:                  OpStore %out_u1 [[ae26]]
  InterlockedExchange(g_tBuffU[u1], u1, out_u1);

// CHECK-NEXT:    [[u1:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %g_tRWBuffU %int_0 [[u1]]
// CHECK-NEXT:    [[u1:%\d+]] = OpLoad %uint %u1
// CHECK-NEXT:   [[add:%\d+]] = OpAtomicIAdd %uint [[ptr]] %uint_1 %uint_0 [[u1]]
// CHECK-NEXT:                  OpStore %out_u1 [[add]]
  InterlockedAdd(g_tRWBuffU[u1], u1, out_u1);
}

