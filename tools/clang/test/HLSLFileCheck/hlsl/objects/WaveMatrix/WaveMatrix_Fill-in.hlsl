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
#ifndef WAVESIZE
#define WAVESIZE
#endif
#ifndef NUMTHREADS
#define NUMTHREADS [NumThreads(64,1,1)]
#endif

#define WML WaveMatrixLeft<COMP, DIMM, DIMN>
#define WMR WaveMatrixRight<COMP, DIMM, DIMN>

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

// CHECK: call void @dx.op.waveMatrix_Fill.[[OLOAD]](i32 228, %dx.types.waveMatrix* nonnull %[[wml]]
// CHECK: call void @dx.op.waveMatrix_Fill.[[OLOAD]](i32 228, %dx.types.waveMatrix* nonnull %[[wmr]]
  left.Fill(1);
  right.Fill(2);
}
