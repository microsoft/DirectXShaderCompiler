// RUN: %dxc -enable-16bit-types -T cs_6_9 -DADD_TY=WMLC %s | FileCheck %s -DADD_TY=2 -DCOMP=9 -DDIMM=16 -DDIMN=16 -check-prefix=CHKIR
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DADD_TY=WMRR %s | FileCheck %s -DADD_TY=3 -DCOMP=9 -DDIMM=16 -DDIMN=16 -check-prefix=CHKIR
// RUN: %dxc -enable-16bit-types -T cs_6_9 -DADD_TY=WMA %s | FileCheck %s -DADD_TY=4 -DCOMP=9 -DDIMM=16 -DDIMN=16 -check-prefix=CHKIR
// RUN: %dxc -enable-16bit-types -T cs_6_9 -ast-dump -DADD_TY=WMLC %s | FileCheck %s -DADD_TY=WaveMatrixLeftColAcc -DCOMP=float -DDIMM=16 -DDIMN=16 -check-prefix=CHKAST
// RUN: %dxc -enable-16bit-types -T cs_6_9 -ast-dump -DADD_TY=WMRR %s | FileCheck %s -DADD_TY=WaveMatrixRightRowAcc -DCOMP=float -DDIMM=16 -DDIMN=16 -check-prefix=CHKAST
// RUN: %dxc -enable-16bit-types -T cs_6_9 -ast-dump -DADD_TY=WMA %s | FileCheck %s -DADD_TY=WaveMatrixAccumulator -DCOMP=float -DDIMM=16 -DDIMN=16 -check-prefix=CHKAST

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
// CHKIR: %[[wma:.*]] = alloca %dx.types.waveMatrix, align 4
// CHKIR: %[[wma_add:.*]] = alloca %dx.types.waveMatrix, align 4
// CHKIR: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.waveMatProps { i8 4, i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
// CHKIR: call void @dx.op.waveMatrix_Annotate(i32 226, %dx.types.waveMatrix* nonnull %[[wma_add]], %dx.types.waveMatProps { i8 [[ADD_TY]], i8 [[COMP]], i32 [[DIMM]], i32 [[DIMN]] })
  WMA acc;
  ADD_TY wma_add;

// CHKAST: CXXMemberCallExpr
// CHKAST-NEXT: MemberExpr
// CHKAST-SAME: .Add
// CHKAST-NEXT: DeclRefExpr
// CHKAST-SAME: 'acc' 'WaveMatrixAccumulator<[[COMP]], [[DIMM]], [[DIMN]]>'
// CHKAST-NEXT: <LValueToRValue>
// CHKAST-NEXT: DeclRefExpr
// CHKAST-SAME: 'wma_add' '[[ADD_TY]]<[[COMP]], [[DIMM]], [[DIMN]]>'

// CHKIR: call void @dx.op.waveMatrix_Accumulate(i32 237, %dx.types.waveMatrix* nonnull %[[wma]], %dx.types.waveMatrix* nonnull %[[wma_add]])
  acc.Add(wma_add);
}
