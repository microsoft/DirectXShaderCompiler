; RUN: %dxv %s | FileCheck %s

; CHECK: InnerCoverage and Coverage are mutually exclusive.

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: alwaysinline nounwind
define void @main(float %b, i32 %c, i32* nocapture readnone dereferenceable(4) %cover) #0 {
entry:
  %0 = call i32 @dx.op.coverage.i32(i32 93)  ; Coverage()
  %1 = call i32 @dx.op.innercoverage.i32(i32 94)  ; InnerCoverage()
  %and = and i32 %1, %0
  call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %and)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.coverage.i32(i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.innercoverage.i32(i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.i32(i32, i32, i32, i8, i32) #2

attributes #0 = { alwaysinline nounwind "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="0" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.valver = !{!1}
!dx.version = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4}
!dx.entryPoints = !{!15}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 0}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{i32 1, void (float, i32, i32*)* @main, !5}
!5 = !{!6, !8, !11, !13}
!6 = !{i32 1, !7, !7}
!7 = !{}
!8 = !{i32 0, !9, !10}
!9 = !{i32 4, !"B", i32 7, i32 13}
!10 = !{i32 0}
!11 = !{i32 0, !12, !10}
!12 = !{i32 4, !"C", i32 7, i32 5}
!13 = !{i32 2, !14, !10}
!14 = !{i32 4, !"SV_Coverage", i32 7, i32 5}
!15 = !{void (float, i32, i32*)* @main, !"main", !16, null, null}
!16 = !{!17, !20, null}
!17 = !{!18, !19}
!18 = !{i32 0, !"B", i8 13, i8 0, !10, i8 1, i32 1, i8 1, i32 0, i8 0, null}
!19 = !{i32 1, !"C", i8 5, i8 0, !10, i8 1, i32 1, i8 1, i32 0, i8 1, null}
!20 = !{!21}
!21 = !{i32 0, !"SV_Coverage", i8 5, i8 14, !10, i8 0, i32 1, i8 1, i32 -1, i8 -1, null}
