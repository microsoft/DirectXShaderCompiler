; RUN: %dxv %s | FileCheck %s

; CHECK: Interpolation mode on A used with eval_* instruction must be linear, linear_centroid, linear_noperspective, linear_noperspective_centroid, linear_sample or linear_noperspective_sample
; CHECK: Interpolation mode on A used with eval_* instruction must be linear, linear_centroid, linear_noperspective, linear_noperspective_centroid, linear_sample or linear_noperspective_sample
; CHECK: Interpolation mode on A used with eval_* instruction must be linear, linear_centroid, linear_noperspective, linear_noperspective_centroid, linear_sample or linear_noperspective_sample


target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @main.flat(<4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>* nocapture readnone) #0 {
entry:
  %RenderTargetGetSampleCount = tail call i32 @dx.op.renderTargetGetSampleCount(i32 80)
  %sub = add i32 %RenderTargetGetSampleCount, -1
  %5 = tail call float @dx.op.evalCentroid.f32(i32 92, i32 0, i32 0, i8 0)
  %6 = tail call float @dx.op.evalCentroid.f32(i32 92, i32 0, i32 0, i8 1)
  %7 = tail call float @dx.op.evalCentroid.f32(i32 92, i32 0, i32 0, i8 2)
  %8 = tail call float @dx.op.evalCentroid.f32(i32 92, i32 0, i32 0, i8 3)
  %9 = tail call float @dx.op.evalSampleIndex.f32(i32 91, i32 0, i32 0, i8 0, i32 %sub)
  %10 = tail call float @dx.op.evalSampleIndex.f32(i32 91, i32 0, i32 0, i8 1, i32 %sub)
  %11 = tail call float @dx.op.evalSampleIndex.f32(i32 91, i32 0, i32 0, i8 2, i32 %sub)
  %12 = tail call float @dx.op.evalSampleIndex.f32(i32 91, i32 0, i32 0, i8 3, i32 %sub)
  %add.i0 = fadd fast float %9, %5
  %add.i1 = fadd fast float %10, %6
  %add.i2 = fadd fast float %11, %7
  %add.i3 = fadd fast float %12, %8
  %13 = tail call float @dx.op.evalSnapped.f32(i32 90, i32 0, i32 0, i8 0, i32 1, i32 2)
  %14 = tail call float @dx.op.evalSnapped.f32(i32 90, i32 0, i32 0, i8 1, i32 1, i32 2)
  %15 = tail call float @dx.op.evalSnapped.f32(i32 90, i32 0, i32 0, i8 2, i32 1, i32 2)
  %16 = tail call float @dx.op.evalSnapped.f32(i32 90, i32 0, i32 0, i8 3, i32 1, i32 2)
  %add5.i0 = fadd fast float %add.i0, %13
  %add5.i1 = fadd fast float %add.i1, %14
  %add5.i2 = fadd fast float %add.i2, %15
  %add5.i3 = fadd fast float %add.i3, %16
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add5.i0)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %add5.i1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %add5.i2)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %add5.i3)
  ret void
}

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.evalSampleIndex.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind readonly
declare i32 @dx.op.renderTargetGetSampleCount(i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.evalCentroid.f32(i32, i32, i32, i8) #1

; Function Attrs: nounwind readnone
declare float @dx.op.evalSnapped.f32(i32, i32, i32, i8, i32, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!18}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{i32 1, void (<4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>*)* @main.flat, !4}
!4 = !{!5, !7, !10, !12, !14, !16}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{i32 0, !8, !9}
!8 = !{i32 4, !"A", i32 7, i32 9}
!9 = !{i32 0}
!10 = !{i32 0, !11, !9}
!11 = !{i32 4, !"B", i32 5, i32 4, i32 7, i32 9}
!12 = !{i32 0, !13, !9}
!13 = !{i32 4, !"C", i32 5, i32 3, i32 7, i32 9}
!14 = !{i32 0, !15, !9}
!15 = !{i32 4, !"D", i32 5, i32 6, i32 7, i32 9}
!16 = !{i32 1, !17, !9}
!17 = !{i32 4, !"SV_TARGET", i32 7, i32 9}
!18 = !{void (<4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>*)* @main.flat, !"", !19, null, null}
!19 = !{!20, !25, null}
!20 = !{!21, !22, !23, !24}
!21 = !{i32 0, !"A", i8 9, i8 0, !9, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!22 = !{i32 1, !"B", i8 9, i8 0, !9, i8 4, i32 1, i8 4, i32 1, i8 0, null}
!23 = !{i32 2, !"C", i8 9, i8 0, !9, i8 3, i32 1, i8 4, i32 2, i8 0, null}
!24 = !{i32 3, !"D", i8 9, i8 0, !9, i8 6, i32 1, i8 4, i32 3, i8 0, null}
!25 = !{!26}
!26 = !{i32 0, !"SV_Target", i8 9, i8 16, !9, i8 0, i32 1, i8 4, i32 0, i8 0, null}
