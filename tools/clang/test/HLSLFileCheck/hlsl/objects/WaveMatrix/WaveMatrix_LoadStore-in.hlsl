// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=9 -DDIMM=16 -DDIMN=16 -DOLOAD=f32
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=float16_t -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=8 -DDIMM=16 -DDIMN=16 -DOLOAD=f16
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=int8_t4_packed -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=17 -DDIMM=16 -DDIMN=16 -DOLOAD=i32
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DCOMP=uint8_t4_packed -DDIMM=16 -DDIMN=16 %s | FileCheck %s -DCOMP=18 -DDIMM=16 -DDIMN=16 -DOLOAD=i32
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

#define WML WaveMatrixLeft<COMP, DIMM, DIMN>
#define WMR WaveMatrixRight<COMP, DIMM, DIMN>

// Should be no addrspacecast from groupshared.
// CHECK-NOT: addrspacecast

groupshared COMP ai512[512];

ByteAddressBuffer buf;
RWByteAddressBuffer rwbuf;

WAVESIZE
NUMTHREADS
void main(uint3 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex)
{
// CHECK: %[[wml:.*]] = alloca %dx.types.waveMatrix, align 4
// CHECK: %[[wmr:.*]] = alloca %dx.types.waveMatrix, align 4
// CHECK: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.waveMatProps { i8 0, i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
// CHECK: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.waveMatProps { i8 1, i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
  WML left;
  WMR right;

  uint n = 0;
#define IDX() (n++*1024)

// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.Handle %{{[^,]+}}, i32 0, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.Handle %{{[^,]+}}, i32 1024, i32 64, i8 16, i1 true)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.Handle %{{[^,]+}}, i32 2048, i32 64, i8 0, i1 true)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.Handle %{{[^,]+}}, i32 3072, i32 64, i8 16, i1 false)
  left.Load(buf, IDX(), STRIDE, false);
  left.Load(buf, IDX(), STRIDE, true, 16);
  left.Load(rwbuf, IDX(), STRIDE, true);
  left.Load(rwbuf, IDX(), STRIDE, false, 16);
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.Handle %{{[^,]+}}, i32 4096, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wml]], %dx.types.Handle %{{[^,]+}}, i32 5120, i32 64, i8 16, i1 true)
  left.Store(rwbuf, IDX(), STRIDE, false);
  left.Store(rwbuf, IDX(), STRIDE, true, 16);
// CHECK: call void @dx.op.waveMatrix_LoadGroupShared.[[OLOAD]](i32 230, %dx.types.waveMatrix* nonnull %[[wml]], {{.+}} addrspace(3)* {{.+}}, i32 0, i32 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreGroupShared.[[OLOAD]](i32 232, %dx.types.waveMatrix* nonnull %[[wml]], {{.+}} addrspace(3)* {{.+}}, i32 32, i32 16, i1 true)
  left.Load(ai512, 0, 16, false);
  left.Store(ai512, 32, 16, true);

// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.Handle %{{[^,]+}}, i32 6144, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.Handle %{{[^,]+}}, i32 7168, i32 64, i8 16, i1 true)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.Handle %{{[^,]+}}, i32 8192, i32 64, i8 0, i1 true)
// CHECK: call void @dx.op.waveMatrix_LoadRawBuf(i32 229, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.Handle %{{[^,]+}}, i32 9216, i32 64, i8 16, i1 false)
  right.Load(buf, IDX(), STRIDE, false);
  right.Load(buf, IDX(), STRIDE, true, 16);
  right.Load(rwbuf, IDX(), STRIDE, true);
  right.Load(rwbuf, IDX(), STRIDE, false, 16);
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.Handle %{{[^,]+}}, i32 10240, i32 64, i8 0, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreRawBuf(i32 231, %dx.types.waveMatrix* nonnull %[[wmr]], %dx.types.Handle %{{[^,]+}}, i32 11264, i32 64, i8 16, i1 true)
  right.Store(rwbuf, IDX(), STRIDE, false);
  right.Store(rwbuf, IDX(), STRIDE, true, 16);
// CHECK: call void @dx.op.waveMatrix_LoadGroupShared.[[OLOAD]](i32 230, %dx.types.waveMatrix* nonnull %[[wmr]], {{.+}} addrspace(3)* {{.+}}, i32 48, i32 16, i1 false)
// CHECK: call void @dx.op.waveMatrix_StoreGroupShared.[[OLOAD]](i32 232, %dx.types.waveMatrix* nonnull %[[wmr]], {{.+}} addrspace(3)* {{.+}}, i32 64, i32 16, i1 true)
  right.Load(ai512, 48, 16, false);
  right.Store(ai512, 64, 16, true);
}
