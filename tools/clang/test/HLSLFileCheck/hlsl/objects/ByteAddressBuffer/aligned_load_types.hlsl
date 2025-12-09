// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float -DALIGN=4 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT32_A4_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float -DALIGN=16 %s | FileCheck %s -check-prefix=CHK_FLOAT32_A16_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float -DALIGN=8 -DSRCRW %s | FileCheck %s -check-prefix=CHK_FLOAT32_A8_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float -DALIGN=32 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT32_A32_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float4 -DALIGN=4 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT32x4_A4_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float4 -DALIGN=16 %s | FileCheck %s -check-prefix=CHK_FLOAT32x4_A16_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float4 -DALIGN=8 -DSRCRW %s | FileCheck %s -check-prefix=CHK_FLOAT32x4_A8_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float4 -DALIGN=64 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT32x4_A64_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float16_t -DALIGN=2 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT16_A2_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float16_t -DALIGN=8 %s | FileCheck %s -check-prefix=CHK_FLOAT16_A8_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float16_t -DALIGN=4 -DSRCRW %s | FileCheck %s -check-prefix=CHK_FLOAT16_A4_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=float16_t -DALIGN=16 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT16_A16_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=double -DALIGN=8 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT64_A8_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=double -DALIGN=32 %s | FileCheck %s -check-prefix=CHK_FLOAT64_A32_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=double -DALIGN=16 -DSRCRW %s | FileCheck %s -check-prefix=CHK_FLOAT64_A16_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=double -DALIGN=64 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_FLOAT64_A64_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint -DALIGN=4 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_UINT32_A4_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint -DALIGN=16 %s | FileCheck %s -check-prefix=CHK_UINT32_A16_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint -DALIGN=8 -DSRCRW %s | FileCheck %s -check-prefix=CHK_UINT32_A8_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint -DALIGN=32 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_UINT32_A32_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint3 -DALIGN=4 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_UINT32x3_A4_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint3 -DALIGN=16 %s | FileCheck %s -check-prefix=CHK_UINT32x3_A16_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint3 -DALIGN=8 -DSRCRW %s | FileCheck %s -check-prefix=CHK_UINT32x3_A8_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint3 -DALIGN=32 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_UINT32x3_A32_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint16_t -DALIGN=2 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_UINT16_A2_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint16_t -DALIGN=8 %s | FileCheck %s -check-prefix=CHK_UINT16_A8_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint16_t -DALIGN=4 -DSRCRW %s | FileCheck %s -check-prefix=CHK_UINT16_A4_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint16_t -DALIGN=16 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_UINT16_A16_RW

// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=int64_t -DALIGN=8 -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_INT64_A8_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=int64_t -DALIGN=32 %s | FileCheck %s -check-prefix=CHK_INT64_A32_RO
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=int64_t -DALIGN=16 -DSRCRW %s | FileCheck %s -check-prefix=CHK_INT64_A16_RW
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=int64_t -DALIGN=64 -DSRCRW -DCHKSTATUS %s | FileCheck %s -check-prefix=CHK_INT64_A64_RW


// CHK_FLOAT32_A4_RO: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 4)
// CHK_FLOAT32_A4_RO: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float undef, float undef, float undef, i8 1, i32 4)

// CHK_FLOAT32_A16_RO: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 16)
// CHK_FLOAT32_A16_RO: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float undef, float undef, float undef, i8 1, i32 16)

// CHK_FLOAT32_A8_RW: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 8)
// CHK_FLOAT32_A8_RW: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float undef, float undef, float undef, i8 1, i32 8)

// CHK_FLOAT32_A32_RW: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 32)
// CHK_FLOAT32_A32_RW: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float undef, float undef, float undef, i8 1, i32 32)

// CHK_FLOAT32x4_A4_RO: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 15, i32 4)
// CHK_FLOAT32x4_A4_RO: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float %{{.*}}, float %{{.*}}, float %{{.*}}, i8 15, i32 4)

// CHK_FLOAT32x4_A16_RO: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 15, i32 16)
// CHK_FLOAT32x4_A16_RO: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float %{{.*}}, float %{{.*}}, float %{{.*}}, i8 15, i32 16)

// CHK_FLOAT32x4_A8_RW: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 15, i32 8)
// CHK_FLOAT32x4_A8_RW: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float %{{.*}}, float %{{.*}}, float %{{.*}}, i8 15, i32 8)

// CHK_FLOAT32x4_A64_RW: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 15, i32 64)
// CHK_FLOAT32x4_A64_RW: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, float %{{.*}}, float %{{.*}}, float %{{.*}}, float %{{.*}}, i8 15, i32 64)

// CHK_FLOAT16_A2_RO: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 2)
// CHK_FLOAT16_A2_RO: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)

// CHK_FLOAT16_A8_RO: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 8)
// CHK_FLOAT16_A8_RO: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 8)

// CHK_FLOAT16_A4_RW: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 4)
// CHK_FLOAT16_A4_RW: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

// CHK_FLOAT16_A16_RW: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 16)
// CHK_FLOAT16_A16_RW: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 16)

// CHK_FLOAT64_A8_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 3, i32 8)
// CHK_FLOAT64_A8_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 8)

// CHK_FLOAT64_A32_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 3, i32 32)
// CHK_FLOAT64_A32_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 32)

// CHK_FLOAT64_A16_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 3, i32 16)
// CHK_FLOAT64_A16_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 16)

// CHK_FLOAT64_A64_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 3, i32 64)
// CHK_FLOAT64_A64_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 64)

// CHK_UINT32_A4_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 4)
// CHK_UINT32_A4_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 undef, i32 undef, i32 undef, i8 1, i32 4)

// CHK_UINT32_A16_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 16)
// CHK_UINT32_A16_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 undef, i32 undef, i32 undef, i8 1, i32 16)

// CHK_UINT32_A8_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 8)
// CHK_UINT32_A8_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 undef, i32 undef, i32 undef, i8 1, i32 8)

// CHK_UINT32_A32_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 32)
// CHK_UINT32_A32_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 undef, i32 undef, i32 undef, i8 1, i32 32)

// CHK_UINT32x3_A4_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 7, i32 4)
// CHK_UINT32x3_A4_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i8 7, i32 4)

// CHK_UINT32x3_A16_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 7, i32 16)
// CHK_UINT32x3_A16_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i8 7, i32 16)

// CHK_UINT32x3_A8_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 7, i32 8)
// CHK_UINT32x3_A8_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i8 7, i32 8)

// CHK_UINT32x3_A32_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 7, i32 32)
// CHK_UINT32x3_A32_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i8 7, i32 32)

// CHK_UINT16_A2_RO: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 2)
// CHK_UINT16_A2_RO: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)

// CHK_UINT16_A8_RO: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 1, i32 8)
// CHK_UINT16_A8_RO: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 8)

// CHK_UINT16_A4_RW: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 4)
// CHK_UINT16_A4_RW: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 4)

// CHK_UINT16_A16_RW: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 1, i32 16)
// CHK_UINT16_A16_RW: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 16)

// CHK_INT64_A8_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 3, i32 8)
// CHK_INT64_A8_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 8)

// CHK_INT64_A32_RO: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_texture_rawbuf, i32 %mul, i32 undef, i8 3, i32 32)
// CHK_INT64_A32_RO: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 32)

// CHK_INT64_A16_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 3, i32 16)
// CHK_INT64_A16_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 16)

// CHK_INT64_A64_RW: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %srcbuf_UAV_rawbuf, i32 %mul, i32 undef, i8 3, i32 64)
// CHK_INT64_A64_RW: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %dstbuf_UAV_rawbuf, i32 %mul, i32 undef, i32 %{{.*}}, i32 %{{.*}}, i32 undef, i32 undef, i8 3, i32 64)


#ifdef SRCRW
RWByteAddressBuffer srcbuf : register(u0);
RWByteAddressBuffer dstbuf : register(u1);
#else
ByteAddressBuffer   srcbuf : register(t0);
RWByteAddressBuffer dstbuf : register(u0);
#endif

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    const uint offset = tid.x * ALIGN;
#ifdef CHKSTATUS
    uint status = 0;
    TY data = srcbuf.AlignedLoad<TY>(offset, ALIGN, status);
    if (!CheckAccessFullyMapped(status)) return;
#else
    TY data = srcbuf.AlignedLoad<TY>(offset, ALIGN);
#endif
    dstbuf.AlignedStore<TY>(offset, ALIGN, data);
}

