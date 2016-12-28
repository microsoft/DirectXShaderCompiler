; RUN: %dxv %s | FileCheck %s

; CHECK:Interpolation mode cannot vary for different cols of a row. Vary at A row 0

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @main.flat(<2 x float>, <2 x float>, <4 x float>* nocapture readnone) #0 {
entry:
  %3 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %4 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)
  %5 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %6 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %5)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %6)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %3)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %4)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!15}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 5, i32 1}
!3 = !{i32 1, void (<2 x float>, <2 x float>, <4 x float>*)* @main.flat, !4}
!4 = !{!5, !7, !10, !13}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{i32 0, !8, !9}
!8 = !{i32 4, !"A", i32 7, i32 9}
!9 = !{i32 0}
!10 = !{i32 0, !11, !12}
!11 = !{i32 4, !"A1", i32 5, i32 3, i32 7, i32 9}
!12 = !{i32 1}
!13 = !{i32 1, !14, !9}
!14 = !{i32 4, !"SV_TARGET", i32 7, i32 9}
!15 = !{void (<2 x float>, <2 x float>, <4 x float>*)* @main.flat, !"", !16, null, null}
!16 = !{!17, !20, null}
!17 = !{!18, !19}
!18 = !{i32 0, !"A", i8 9, i8 0, !9, i8 2, i32 1, i8 2, i32 0, i8 0, null}
!19 = !{i32 1, !"A", i8 9, i8 0, !12, i8 3, i32 1, i8 2, i32 0, i8 2, null}
!20 = !{!21}
!21 = !{i32 0, !"SV_Target", i8 9, i8 16, !9, i8 0, i32 1, i8 4, i32 0, i8 0, null}
