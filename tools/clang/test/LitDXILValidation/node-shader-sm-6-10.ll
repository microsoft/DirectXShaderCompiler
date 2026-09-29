; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

; Node shaders are not valid in shader model 6.10, even when the entry point
; contains no DXIL calls.

; CHECK: Function: Node: error: Shader stage 'node' not valid in shader model lib_6_10.
; CHECK: Validation failed.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @Node() {
  ret void
}

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!7, !9}

!0 = !{!"custom IR"}
!1 = !{i32 1, i32 10}
!2 = !{!"lib", i32 6, i32 10}
!3 = !{i32 1, void ()* @Node, !4}
!4 = !{!5}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{null, !"", null, null, !8}
!8 = !{i32 0, i64 524288}
!9 = !{void ()* @Node, !"Node", null, null, !10}
!10 = !{i32 8, i32 15, i32 13, i32 1, i32 15, !11, i32 16, i32 -1, i32 4, !12, i32 5, !13}
!11 = !{!"Node", i32 0}
!12 = !{i32 1, i32 1, i32 1}
!13 = !{i32 0}
