; RUN: %dxv %s | FileCheck %s

; CHECK: Invalid interpolation mode for 'C'
; CHECK: SV_Depth must be float
; CHECK: External function 'dxil.op.loadInput.f32' is not a DXIL function
; CHECK: External function 'dx.op.loadInput.f32' is unused

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @main.flat(float, float, i32* nocapture readnone) #0 {
entry:
  %3 = call float @dxil.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %conv = fptosi float %3 to i32
  call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %conv)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dxil.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.i32(i32, i32, i32, i8, i32) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!14}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 5, i32 1}
!3 = !{i32 1, void (float, float, i32*)* @main.flat, !4}
!4 = !{!5, !7, !10, !12}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{i32 0, !8, !9}
!8 = !{i32 4, !"B", i32 7, i32 13}
!9 = !{i32 0}
!10 = !{i32 0, !11, !9}
!11 = !{i32 4, !"C", i32 7, i32 9}
!12 = !{i32 1, !13, !9}
!13 = !{i32 4, !"SV_DEPTH", i32 7, i32 4}
!14 = !{void (float, float, i32*)* @main.flat, !"", !15, null, null}
!15 = !{!16, !19, null}
!16 = !{!17, !18}
!17 = !{i32 0, !"B", i8 13, i8 0, !9, i8 1, i32 1, i8 1, i32 0, i8 0, null}
!18 = !{i32 1, !"C", i8 9, i8 0, !9, i8 8, i32 1, i8 1, i32 1, i8 0, null}
!19 = !{!20}
!20 = !{i32 0, !"SV_Depth", i8 4, i8 17, !9, i8 0, i32 1, i8 1, i32 -1, i8 -1, null}
