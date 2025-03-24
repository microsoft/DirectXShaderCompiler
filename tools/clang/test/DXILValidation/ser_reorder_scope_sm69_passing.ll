; RUN: %dxv %s | FileCheck %s

; CHECK: Validation succeeded.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @"\01?main@@YAXXZ"() #0 {
  call void @dx.op.barrierByMemoryType(i32 244, i32 1, i32 8)  ; BarrierByMemoryType(MemoryTypeFlags,SemanticFlags)
  ret void
}

; Function Attrs: noduplicate nounwind
declare void @dx.op.barrierByMemoryType(i32, i32, i32) #1

attributes #0 = { nounwind }
attributes #1 = { noduplicate nounwind }

!dx.version = !{!0}
!dx.valver = !{!0}
!dx.shaderModel = !{!1}
!dx.typeAnnotations = !{!2}
!dx.entryPoints = !{!6, !7}

!0 = !{i32 1, i32 9}
!1 = !{!"lib", i32 6, i32 9}
!2 = !{i32 1, void ()* @"\01?main@@YAXXZ", !3}
!3 = !{!4}
!4 = !{i32 1, !5, !5}
!5 = !{}
!6 = !{null, !"", null, null, null}
!7 = !{void ()* @"\01?main@@YAXXZ", !"\01?main@@YAXXZ", null, null, !8}
!8 = !{i32 8, i32 7, i32 5, !9}
!9 = !{i32 0}

