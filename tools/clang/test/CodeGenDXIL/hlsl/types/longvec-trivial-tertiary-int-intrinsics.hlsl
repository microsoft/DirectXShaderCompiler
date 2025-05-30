// RUN: %dxc -T cs_6_9 -enable-16bit-types -DFUNC=mad   -DOP=48 -DUOP=49 -DNUM=7    %s | FileCheck %s
// RUN: %dxc -T cs_6_9 -enable-16bit-types -DFUNC=mad   -DOP=48 -DUOP=49 -DNUM=1022 %s | FileCheck %s

#ifndef UOP
#define UOP OP
#endif

// Test vector-enabled tertiary intrinsics that take signed and unsigned integer parameters of
// different widths and are "trivial" in that they can be implemented with a single call
// instruction with the same parameter and return types.

RWByteAddressBuffer buf;

// CHECK-DAG: %dx.types.ResRet.[[STY:v[0-9]*i16]] = type { <[[NUM:[0-9]*]] x i16>
// CHECK-DAG: %dx.types.ResRet.[[ITY:v[0-9]*i32]] = type { <[[NUM]] x i32>
// CHECK-DAG: %dx.types.ResRet.[[LTY:v[0-9]*i64]] = type { <[[NUM]] x i64>

[numthreads(8,1,1)]
void main() {

  // Capture opcode numbers.
  // CHECK: [[buf:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle [[buf]], i32 888, i32 undef, i32 [[OP:[0-9]*]]
  buf.Store(888, OP);

  // CHECK: [[buf:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle [[buf]], i32 999, i32 undef, i32 [[UOP:[0-9]*]]
  buf.Store(999, UOP);

  // CHECK: [[buf:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })

  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[STY]] @dx.op.rawBufferVectorLoad.[[STY]](i32 303, %dx.types.Handle [[buf]], i32 0
  // CHECK: [[svec1:%.*]] = extractvalue %dx.types.ResRet.[[STY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[STY]] @dx.op.rawBufferVectorLoad.[[STY]](i32 303, %dx.types.Handle [[buf]], i32 512
  // CHECK: [[svec2:%.*]] = extractvalue %dx.types.ResRet.[[STY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[STY]] @dx.op.rawBufferVectorLoad.[[STY]](i32 303, %dx.types.Handle [[buf]], i32 1024
  // CHECK: [[svec3:%.*]] = extractvalue %dx.types.ResRet.[[STY]] [[ld]], 0
  vector<int16_t, NUM> sVec1 = buf.Load<vector<int16_t, NUM> >(0);
  vector<int16_t, NUM> sVec2 = buf.Load<vector<int16_t, NUM> >(512);
  vector<int16_t, NUM> sVec3 = buf.Load<vector<int16_t, NUM> >(1024);

  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[STY]] @dx.op.rawBufferVectorLoad.[[STY]](i32 303, %dx.types.Handle [[buf]], i32 1025
  // CHECK: [[usvec1:%.*]] = extractvalue %dx.types.ResRet.[[STY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[STY]] @dx.op.rawBufferVectorLoad.[[STY]](i32 303, %dx.types.Handle [[buf]], i32 1536
  // CHECK: [[usvec2:%.*]] = extractvalue %dx.types.ResRet.[[STY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[STY]] @dx.op.rawBufferVectorLoad.[[STY]](i32 303, %dx.types.Handle [[buf]], i32 2048
  // CHECK: [[usvec3:%.*]] = extractvalue %dx.types.ResRet.[[STY]] [[ld]], 0
  vector<uint16_t, NUM> usVec1 = buf.Load<vector<uint16_t, NUM> >(1025);
  vector<uint16_t, NUM> usVec2 = buf.Load<vector<uint16_t, NUM> >(1536);
  vector<uint16_t, NUM> usVec3 = buf.Load<vector<uint16_t, NUM> >(2048);

  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[ITY]] @dx.op.rawBufferVectorLoad.[[ITY]](i32 303, %dx.types.Handle [[buf]], i32 2049
  // CHECK: [[ivec1:%.*]] = extractvalue %dx.types.ResRet.[[ITY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[ITY]] @dx.op.rawBufferVectorLoad.[[ITY]](i32 303, %dx.types.Handle [[buf]], i32 2560
  // CHECK: [[ivec2:%.*]] = extractvalue %dx.types.ResRet.[[ITY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[ITY]] @dx.op.rawBufferVectorLoad.[[ITY]](i32 303, %dx.types.Handle [[buf]], i32 3072
  // CHECK: [[ivec3:%.*]] = extractvalue %dx.types.ResRet.[[ITY]] [[ld]], 0
  vector<int, NUM> iVec1 = buf.Load<vector<int, NUM> >(2049);
  vector<int, NUM> iVec2 = buf.Load<vector<int, NUM> >(2560);
  vector<int, NUM> iVec3 = buf.Load<vector<int, NUM> >(3072);

  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[ITY]] @dx.op.rawBufferVectorLoad.[[ITY]](i32 303, %dx.types.Handle [[buf]], i32 3073
  // CHECK: [[uivec1:%.*]] = extractvalue %dx.types.ResRet.[[ITY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[ITY]] @dx.op.rawBufferVectorLoad.[[ITY]](i32 303, %dx.types.Handle [[buf]], i32 3584
  // CHECK: [[uivec2:%.*]] = extractvalue %dx.types.ResRet.[[ITY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[ITY]] @dx.op.rawBufferVectorLoad.[[ITY]](i32 303, %dx.types.Handle [[buf]], i32 4096
  // CHECK: [[uivec3:%.*]] = extractvalue %dx.types.ResRet.[[ITY]] [[ld]], 0
  vector<uint, NUM> uiVec1 = buf.Load<vector<uint, NUM> >(3073);
  vector<uint, NUM> uiVec2 = buf.Load<vector<uint, NUM> >(3584);
  vector<uint, NUM> uiVec3 = buf.Load<vector<uint, NUM> >(4096);

  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[LTY]] @dx.op.rawBufferVectorLoad.[[LTY]](i32 303, %dx.types.Handle [[buf]], i32 4097
  // CHECK: [[lvec1:%.*]] = extractvalue %dx.types.ResRet.[[LTY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[LTY]] @dx.op.rawBufferVectorLoad.[[LTY]](i32 303, %dx.types.Handle [[buf]], i32 4608
  // CHECK: [[lvec2:%.*]] = extractvalue %dx.types.ResRet.[[LTY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[LTY]] @dx.op.rawBufferVectorLoad.[[LTY]](i32 303, %dx.types.Handle [[buf]], i32 5120
  // CHECK: [[lvec3:%.*]] = extractvalue %dx.types.ResRet.[[LTY]] [[ld]], 0
  vector<int64_t, NUM> lVec1 = buf.Load<vector<int64_t, NUM> >(4097);
  vector<int64_t, NUM> lVec2 = buf.Load<vector<int64_t, NUM> >(4608);
  vector<int64_t, NUM> lVec3 = buf.Load<vector<int64_t, NUM> >(5120);

  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[LTY]] @dx.op.rawBufferVectorLoad.[[LTY]](i32 303, %dx.types.Handle [[buf]], i32 5121
  // CHECK: [[ulvec1:%.*]] = extractvalue %dx.types.ResRet.[[LTY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[LTY]] @dx.op.rawBufferVectorLoad.[[LTY]](i32 303, %dx.types.Handle [[buf]], i32 5632
  // CHECK: [[ulvec2:%.*]] = extractvalue %dx.types.ResRet.[[LTY]] [[ld]], 0
  // CHECK: [[ld:%.*]] = call %dx.types.ResRet.[[LTY]] @dx.op.rawBufferVectorLoad.[[LTY]](i32 303, %dx.types.Handle [[buf]], i32 6144
  // CHECK: [[ulvec3:%.*]] = extractvalue %dx.types.ResRet.[[LTY]] [[ld]], 0
  vector<uint64_t, NUM> ulVec1 = buf.Load<vector<uint64_t, NUM> >(5121);
  vector<uint64_t, NUM> ulVec2 = buf.Load<vector<uint64_t, NUM> >(5632);
  vector<uint64_t, NUM> ulVec3 = buf.Load<vector<uint64_t, NUM> >(6144);

  // Test simple matching type overloads.
  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  // CHECK: call <[[NUM]] x i16> @dx.op.tertiary.[[STY]](i32 [[OP]], <[[NUM]] x i16> [[svec1]], <[[NUM]] x i16> [[svec2]], <[[NUM]] x i16> [[svec3]])
  vector<int16_t, NUM> sRes = FUNC(sVec1, sVec2, sVec3);

  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  // CHECK: call <[[NUM]] x i16> @dx.op.tertiary.[[STY]](i32 [[UOP]], <[[NUM]] x i16> [[usvec1]], <[[NUM]] x i16> [[usvec2]], <[[NUM]] x i16> [[usvec3]])
  vector<uint16_t, NUM> usRes = FUNC(usVec1, usVec2, usVec3);

  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  // CHECK: call <[[NUM]] x i32> @dx.op.tertiary.[[ITY]](i32 [[OP]], <[[NUM]] x i32> [[ivec1]], <[[NUM]] x i32> [[ivec2]], <[[NUM]] x i32> [[ivec3]])
  vector<int, NUM> iRes = FUNC(iVec1, iVec2, iVec3);

  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  // CHECK: call <[[NUM]] x i32> @dx.op.tertiary.[[ITY]](i32 [[UOP]], <[[NUM]] x i32> [[uivec1]], <[[NUM]] x i32> [[uivec2]], <[[NUM]] x i32> [[uivec3]])
  vector<uint, NUM> uiRes = FUNC(uiVec1, uiVec2, uiVec3);

  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  // CHECK: call <[[NUM]] x i64> @dx.op.tertiary.[[LTY]](i32 [[OP]], <[[NUM]] x i64> [[lvec1]], <[[NUM]] x i64> [[lvec2]], <[[NUM]] x i64> [[lvec3]])
  vector<int64_t, NUM> lRes = FUNC(lVec1, lVec2, lVec3);

  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  // CHECK: call <[[NUM]] x i64> @dx.op.tertiary.[[LTY]](i32 [[UOP]], <[[NUM]] x i64> [[ulvec1]], <[[NUM]] x i64> [[ulvec2]], <[[NUM]] x i64> [[ulvec3]])
  vector<uint64_t, NUM> ulRes = FUNC(ulVec1, ulVec2, ulVec3);

  // CHECK-NOT: extractelement
  // CHECK-NOT: insertelement
  buf.Store<vector<int16_t, NUM> >(0, sRes);
  buf.Store<vector<uint16_t, NUM> >(1024, usRes);
  buf.Store<vector<int, NUM> >(2048, iRes);
  buf.Store<vector<uint, NUM> >(3072, uiRes);
  buf.Store<vector<int64_t, NUM> >(4096, lRes);
  buf.Store<vector<uint64_t, NUM> >(5120, ulRes);
}
