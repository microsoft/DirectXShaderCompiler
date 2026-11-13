; RUN: %dxopt %s -hlsl-passes-resume -dxil-cond-mem2reg -S | FileCheck %s
; Test that conditionalmem2reg does not scalarize precise native vectors
; as it would pre-6.9 as part of keeping their allocas around to maintain
; the precise information.

; The checks are just confirming that the precise calls are preserved

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

; Function Attrs: nounwind
define void @main(<4 x float>* noalias %arg, <4 x float> %arg1, <4 x float> %arg2, <4 x float> %arg3) #0 {
entry:
  %shift.addr = alloca <4 x float>, align 4, !dx.temp !9
  %scale.addr = alloca <4 x float>, align 4, !dx.temp !9
  %pos.addr = alloca <4 x float>, align 4, !dx.temp !9

  ; Confirm that position is the only alloca that is preserved
  ; CHECK-NOT: alloca
  ; CHECK: %position = alloca <4 x float>, align 4, !dx.precise
  ; CHECK-NOT: alloca
  %position = alloca <4 x float>, align 4, !dx.precise !23
  store <4 x float> %arg3, <4 x float>* %shift.addr, align 4, !tbaa !24
  store <4 x float> %arg2, <4 x float>* %scale.addr, align 4, !tbaa !24
  store <4 x float> %arg1, <4 x float>* %pos.addr, align 4, !tbaa !24
  %tmp = bitcast <4 x float>* %position to i8*
  call void @llvm.lifetime.start(i64 16, i8* %tmp) #0
  %tmp4 = load <4 x float>, <4 x float>* %pos.addr, align 4, !tbaa !24

  ; CHECK: call void @"dx.attribute.precise.<4 x float>"(<4 x float> %arg1)
  ; CHECK-NEXT: store <4 x float> %arg1, <4 x float>* %position, align 4
  call void @"dx.attribute.precise.<4 x float>"(<4 x float> %tmp4)
  store <4 x float> %tmp4, <4 x float>* %position, align 4, !tbaa !24
  %tmp5 = load <4 x float>, <4 x float>* %position, align 4, !tbaa !24
  %tmp6 = load <4 x float>, <4 x float>* %scale.addr, align 4, !tbaa !24
  %mul = fmul <4 x float> %tmp5, %tmp6

  ; CHECK: call void @"dx.attribute.precise.<4 x float>"(<4 x float> %mul)
  ; CHECK-NEXT: store <4 x float> %mul, <4 x float>* %position, align 4
  call void @"dx.attribute.precise.<4 x float>"(<4 x float> %mul)
  store <4 x float> %mul, <4 x float>* %position, align 4, !tbaa !24
  %tmp7 = load <4 x float>, <4 x float>* %position, align 4
  %tmp8 = extractelement <4 x float> %tmp7, i32 2
  %add = fadd float %tmp8, 0x3F847AE140000000
  %tmp9 = getelementptr <4 x float>, <4 x float>* %position, i32 0, i32 2

  ; CHECK: call void @dx.attribute.precise.float(float %add)
  ; CHECK-NEXT: store float %add, float* %tmp9
  call void @dx.attribute.precise.float(float %add)
  store float %add, float* %tmp9
  %tmp10 = load <4 x float>, <4 x float>* %shift.addr, align 4, !tbaa !24
  %tmp11 = load <4 x float>, <4 x float>* %position, align 4, !tbaa !24
  %add1 = fadd <4 x float> %tmp11, %tmp10

  ; CHECK: call void @"dx.attribute.precise.<4 x float>"(<4 x float> %add1)
  ; CHECK-NEXT: store <4 x float> %add1, <4 x float>* %position, align 4
  call void @"dx.attribute.precise.<4 x float>"(<4 x float> %add1)
  store <4 x float> %add1, <4 x float>* %position, align 4, !tbaa !24
  %tmp12 = load <4 x float>, <4 x float>* %position, align 4, !tbaa !24
  %tmp13 = bitcast <4 x float>* %position to i8*
  call void @llvm.lifetime.end(i64 16, i8* %tmp13) #0
  store <4 x float> %tmp12, <4 x float>* %arg
  ret void
}

declare void @"dx.attribute.precise.<4 x float>"(<4 x float>) #1

declare void @dx.attribute.precise.float(float) #1

attributes #0 = { nounwind }
attributes #1 = { "dx.precise" }

!pauseresume = !{!1}
!dx.version = !{!3}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}
!dx.typeAnnotations = !{!6}
!dx.entryPoints = !{!19}
!dx.fnprops = !{!20}
!dx.options = !{!21, !22}

!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!3 = !{i32 1, i32 9}
!4 = !{i32 1, i32 10}
!5 = !{!"vs", i32 6, i32 9}
!6 = !{i32 1, void (<4 x float>*, <4 x float>, <4 x float>, <4 x float>)* @main, !7}
!7 = !{!8, !10, !13, !15, !17}
!8 = !{i32 0, !9, !9}
!9 = !{}
!10 = !{i32 1, !11, !12}
!11 = !{i32 8, i1 true, i32 4, !"SV_Position", i32 7, i32 9}
!12 = !{i32 0}
!13 = !{i32 0, !14, !12}
!14 = !{i32 4, !"POSITION", i32 7, i32 9}
!15 = !{i32 0, !16, !12}
!16 = !{i32 4, !"SCL", i32 7, i32 9}
!17 = !{i32 0, !18, !12}
!18 = !{i32 4, !"OFF", i32 7, i32 9}
!19 = !{void (<4 x float>*, <4 x float>, <4 x float>, <4 x float>)* @main, !"main", null, null, null}
!20 = !{void (<4 x float>*, <4 x float>, <4 x float>, <4 x float>)* @main, i32 1}
!21 = !{i32 64}
!22 = !{i32 -1}
!23 = !{i32 1}
!24 = !{!25, !25, i64 0}
!25 = !{!"omnipotent char", !26, i64 0}
!26 = !{!"Simple C/C++ TBAA"}
