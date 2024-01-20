// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=9 -DDIMM=16 -DDIMN=16 -DOLOAD=f32
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float16_t -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=8 -DDIMM=16 -DDIMN=16 -DOLOAD=f16
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=int -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=4 -DDIMM=16 -DDIMN=16 -DOLOAD=i32
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float -DDIMM=64 -DDIMN=16 %s | FileCheck %s -DCOMP=9 -DDIMM=64 -DDIMN=16 -DOLOAD=f32
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float -DDIMM=16 -DDIMN=64 %s | FileCheck %s -DCOMP=9 -DDIMM=16 -DDIMN=64 -DOLOAD=f32
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float -DDIMM=64 -DDIMN=64 %s | FileCheck %s -DCOMP=9 -DDIMM=64 -DDIMN=64 -DOLOAD=f32

// CHECK: ; Note: shader requires additional functionality:
// CHECK: ;       Wave level operations
// CHECK: ;       Wave Matrix

// CHECK: define void @main()

#ifndef COMP
#define COMP float
#endif
#ifndef DIMM
#define DIMM 16
#endif
#ifndef DIMN
#define DIMN 16
#endif
#ifndef STRIDE
#define STRIDE 64
#endif
#ifndef WAVESIZE
#define WAVESIZE
#endif
#ifndef NUMTHREADS
#define NUMTHREADS [NumThreads(64,1,1)]
#endif

#define WMLC WaveMatrixLeftColAcc<COMP, DIMM, DIMN>
#define WMRR WaveMatrixRightRowAcc<COMP, DIMM, DIMN>
#define WMA WaveMatrixAccumulator<COMP, DIMM, DIMN>

// Should be no addrspacecast from groupshared.
// CHECK-NOT: addrspacecast

groupshared COMP ai512[512];

ByteAddressBuffer buf;
RWByteAddressBuffer rwbuf;

WAVESIZE
NUMTHREADS
void main(uint3 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex)
{
// CHECK: %[[wmlc:.*]] = alloca %dx.types.waveMatrix, align 4
// CHECK: %[[wmrr:.*]] = alloca %dx.types.waveMatrix, align 4
// CHECK: %[[wma:.*]] = alloca %dx.types.waveMatrix, align 4
// CHECK: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.waveMatProps { i8 2, i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
// CHECK: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.waveMatProps { i8 3, i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
// CHECK: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.waveMatProps { i8 4, i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
  WMLC leftcol;
  WMRR rightrow;
  WMA acc;

  uint n = 0;
#define IDX() (n++*1024)

// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.Handle %{{[^,]+}}, i32 0, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.Handle %{{[^,]+}}, i32 1024, i32 64, i8 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.Handle %{{[^,]+}}, i32 2048, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.Handle %{{[^,]+}}, i32 3072, i32 64, i8 16, i1 false)
  leftcol.Load(buf, IDX(), STRIDE);
  leftcol.Load(buf, IDX(), STRIDE, 16);
  leftcol.Load(rwbuf, IDX(), STRIDE);
  leftcol.Load(rwbuf, IDX(), STRIDE, 16);
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.Handle %{{[^,]+}}, i32 4096, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wmlc]], %dx.types.Handle %{{[^,]+}}, i32 5120, i32 64, i8 16, i1 false)
  leftcol.Store(rwbuf, IDX(), STRIDE);
  leftcol.Store(rwbuf, IDX(), STRIDE, 16);
// CHECK: call void @dx.op.waveMatrix_LoadGroupShared.[[OLOAD]](i32 230, %dx.types.waveMatrix* nonnull %[[wmlc]], {{.+}} addrspace(3)* {{.+}}, i32 0, i32 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreGroupShared.[[OLOAD]](i32 232, %dx.types.waveMatrix* nonnull %[[wmlc]], {{.+}} addrspace(3)* {{.+}}, i32 32, i32 16, i1 false)
  leftcol.Load(ai512, 0, 16);
  leftcol.Store(ai512, 32, 16);

// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.Handle %{{[^,]+}}, i32 6144, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.Handle %{{[^,]+}}, i32 7168, i32 64, i8 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.Handle %{{[^,]+}}, i32 8192, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.Handle %{{[^,]+}}, i32 9216, i32 64, i8 16, i1 false)
  rightrow.Load(buf, IDX(), STRIDE);
  rightrow.Load(buf, IDX(), STRIDE, 16);
  rightrow.Load(rwbuf, IDX(), STRIDE);
  rightrow.Load(rwbuf, IDX(), STRIDE, 16);
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.Handle %{{[^,]+}}, i32 10240, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wmrr]], %dx.types.Handle %{{[^,]+}}, i32 11264, i32 64, i8 16, i1 false)
  rightrow.Store(rwbuf, IDX(), STRIDE);
  rightrow.Store(rwbuf, IDX(), STRIDE, 16);
// CHECK: call void @dx.op.waveMatrix_LoadGroupShared.[[OLOAD]](i32 230, %dx.types.waveMatrix* nonnull %[[wmrr]], {{.+}} addrspace(3)* {{.+}}, i32 48, i32 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreGroupShared.[[OLOAD]](i32 232, %dx.types.waveMatrix* nonnull %[[wmrr]], {{.+}} addrspace(3)* {{.+}}, i32 64, i32 16, i1 false)
  rightrow.Load(ai512, 48, 16);
  rightrow.Store(ai512, 64, 16);

// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.Handle %{{[^,]+}}, i32 12288, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.Handle %{{[^,]+}}, i32 13312, i32 64, i8 16, i1 true)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.Handle %{{[^,]+}}, i32 14336, i32 64, i8 0, i1 true)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.Handle %{{[^,]+}}, i32 15360, i32 64, i8 16, i1 false)
  acc.Load(buf, IDX(), STRIDE, false);
  acc.Load(buf, IDX(), STRIDE, true, 16);
  acc.Load(rwbuf, IDX(), STRIDE, true);
  acc.Load(rwbuf, IDX(), STRIDE, false, 16);
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.Handle %{{[^,]+}}, i32 16384, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.Handle %{{[^,]+}}, i32 17408, i32 64, i8 16, i1 true)
  acc.Store(rwbuf, IDX(), STRIDE, false);
  acc.Store(rwbuf, IDX(), STRIDE, true, 16);
// CHECK: call void @dx.op.waveMatrix_LoadGroupShared.[[OLOAD]](i32 230, %dx.types.waveMatrix* nonnull %[[wma]], {{.+}} addrspace(3)* {{.+}}, i32 80, i32 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreGroupShared.[[OLOAD]](i32 232, %dx.types.waveMatrix* nonnull %[[wma]], {{.+}} addrspace(3)* {{.+}}, i32 96, i32 16, i1 true)
  acc.Load(ai512, 80, 16, false);
  acc.Store(ai512, 96, 16, true);
}
