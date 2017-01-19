; RUN: %dxv %s | FileCheck %s

; CHECK: InnerCoverage and Coverage are mutually exclusive.

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: alwaysinline nounwind
define void @main(float %b, float %c, i32 %inner, i32* nocapture readnone dereferenceable(4) %cover) #0 {
entry:
  %0 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 0, i32 undef)
  call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %0)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.i32(i32, i32, i32, i8, i32) #2

attributes #0 = { alwaysinline nounwind "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="0" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!16}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{i32 1, void (float, float, i32, i32*)* @main, !4}
!4 = !{!5, !7, !10, !12, !14}
!5 = !{i32 1, !6, !6}
!6 = !{}
!7 = !{i32 0, !8, !9}
!8 = !{i32 4, !"B", i32 7, i32 13}
!9 = !{i32 0}
!10 = !{i32 0, !11, !9}
!11 = !{i32 4, !"C", i32 7, i32 9}
!12 = !{i32 0, !13, !9}
!13 = !{i32 4, !"SV_InnerCoverage", i32 7, i32 5}
!14 = !{i32 2, !15, !9}
!15 = !{i32 4, !"SV_Coverage", i32 7, i32 5}
!16 = !{void (float, float, i32, i32*)* @main, !"", !17, null, !25}
!17 = !{!18, !23, null}
!18 = !{!19, !20, !21, !22}
!19 = !{i32 0, !"B", i8 13, i8 0, !9, i8 1, i32 1, i8 1, i32 0, i8 0, null}
!20 = !{i32 1, !"C", i8 9, i8 0, !9, i8 2, i32 1, i8 1, i32 1, i8 0, null}
!21 = !{i32 2, !"SV_InnerCoverage", i8 5, i8 15, !9, i8 1, i32 1, i8 1, i32 2, i8 0, null}
!22 = !{i32 3, !"SV_Coverage", i8 5, i8 14, !9, i8 0, i32 1, i8 1, i32 3, i8 0, null}
!23 = !{!24}
!24 = !{i32 0, !"SV_Coverage", i8 5, i8 14, !9, i8 0, i32 1, i8 1, i32 0, i8 0, null}
!25 = !{i32 0, i64 1024}
