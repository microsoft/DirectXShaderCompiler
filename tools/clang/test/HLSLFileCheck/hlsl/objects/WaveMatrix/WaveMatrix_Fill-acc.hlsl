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
#ifndef WAVESIZE
#define WAVESIZE
#endif
#ifndef NUMTHREADS
#define NUMTHREADS [NumThreads(64,1,1)]
#endif

#define WMLC WaveMatrixLeftColAcc<COMP, DIMM, DIMN>
#define WMRR WaveMatrixRightRowAcc<COMP, DIMM, DIMN>
#define WMA WaveMatrixAccumulator<COMP, DIMM, DIMN>

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

// CHECK: call void @dx.op.waveMatrix_Fill.[[OLOAD]](i32 228, %dx.types.waveMatrix* nonnull %[[wmlc]]
  leftcol.Fill(1);
// CHECK: call void @dx.op.waveMatrix_Fill.[[OLOAD]](i32 228, %dx.types.waveMatrix* nonnull %[[wmrr]]
  rightrow.Fill(2);
// CHECK: call void @dx.op.waveMatrix_Fill.[[OLOAD]](i32 228, %dx.types.waveMatrix* nonnull %[[wma]]
  acc.Fill(3);
}
