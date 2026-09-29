; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

; CHECK: Function: main: error: Instructions must be of an allowed type.
; CHECK: note: at '%value = insertvalue %Pair undef, i32 1, 0'
; CHECK: Validation failed.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%Pair = type { i32, i32 }

define void @main() {
  %value = insertvalue %Pair undef, i32 1, 0
  ret void
}

!dx.version = !{!0}
!dx.valver = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}

!0 = !{i32 1, i32 9}
!1 = !{!"cs", i32 6, i32 9}
!2 = !{void ()* @main, !"main", null, null, !3}
!3 = !{i32 0, i64 0, i32 4, !4}
!4 = !{i32 4, i32 1, i32 1}
