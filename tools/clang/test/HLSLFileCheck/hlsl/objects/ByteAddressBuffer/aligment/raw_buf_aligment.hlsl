// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t  %s | FileCheck %s -check-prefix=CHK_FLT16
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t2  %s | FileCheck %s -check-prefix=CHK_FLT16_V2
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t3  %s | FileCheck %s -check-prefix=CHK_FLT16_V3
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t4  %s | FileCheck %s -check-prefix=CHK_FLT16_V4
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t1x1  %s | FileCheck %s -check-prefix=CHK_FLT16_M1x1
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t2x3  %s | FileCheck %s -check-prefix=CHK_FLT16_M2x3
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t3x3  %s | FileCheck %s -check-prefix=CHK_FLT16_M3x3
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t4x3  %s | FileCheck %s -check-prefix=CHK_FLT16_M4x3
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DT=float16_t4x4  %s | FileCheck %s -check-prefix=CHK_FLT16_M4x4


ByteAddressBuffer srv;
RWByteAddressBuffer uav;

void main(uint i : IN0)
{

// CHK_FLT16: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 1, i32 2)
// CHK_FLT16: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLT16_V2: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 3, i32 2)
// CHK_FLT16_V2: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

// CHK_FLT16_V3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 7, i32 2)
// CHK_FLT16_V3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)

// CHK_FLT16_V4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_V4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)

// CHK_FLT16_M1x1: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 1, i32 2)
// CHK_FLT16_M1x1: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLT16_M2x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M2x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 3, i32 2)
// CHK_FLT16_M2x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M2x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 1, i32 2)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)

// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
      uav.Store(i, srv.Load<T>(i));

// CHK_FLT16: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 1, i32 4)
// CHK_FLT16: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

// CHK_FLT16_V2: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 3, i32 4)
// CHK_FLT16_V2: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)

// CHK_FLT16_V3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 7, i32 4)
// CHK_FLT16_V3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 4)

// CHK_FLT16_V4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 15, i32 4)
// CHK_FLT16_V4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)

// CHK_FLT16_M1x1: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 1, i32 4)
// CHK_FLT16_M1x1: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

// CHK_FLT16_M2x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 15, i32 4)
// CHK_FLT16_M2x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 8, i32 undef, i8 3, i32 4)
// CHK_FLT16_M2x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M2x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 8, i32 undef, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)

// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 15, i32 4)
// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 8, i32 undef, i8 15, i32 4)
// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 16, i32 undef, i8 1, i32 4)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 8, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 16, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 8, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 16, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 8, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 16, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)

// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 8, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 16, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 24, i32 undef, i8 15, i32 4)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 8, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 16, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 24, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
      uav.Store(0, srv.Load<T>(0));

// CHK_FLT16: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 1, i32 2)
// CHK_FLT16: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLT16_V2: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 3, i32 2)
// CHK_FLT16_V2: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

// CHK_FLT16_V3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 7, i32 2)
// CHK_FLT16_V3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)

// CHK_FLT16_V4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 15, i32 2)
// CHK_FLT16_V4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)

// CHK_FLT16_M1x1: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 1, i32 2)
// CHK_FLT16_M1x1: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLT16_M2x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 15, i32 2)
// CHK_FLT16_M2x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 10, i32 undef, i8 3, i32 2)
// CHK_FLT16_M2x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M2x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 10, i32 undef, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 15, i32 2)
// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 10, i32 undef, i8 15, i32 2)
// CHK_FLT16_M3x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 18, i32 undef, i8 1, i32 2)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 10, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M3x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 18, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 10, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x3: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 18, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 10, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x3: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 18, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)

// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 2, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 10, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 18, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 26, i32 undef, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 2, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 10, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 18, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
// CHK_FLT16_M4x4: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 26, i32 undef, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
      uav.Store(2, srv.Load<T>(2));
}
