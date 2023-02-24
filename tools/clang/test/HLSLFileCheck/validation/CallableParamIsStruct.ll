; RUN: %dxv %s | FileCheck %s

; CHECK: Function: BadCallable: error: Argument 'f' must be a struct type for callable shader function 'BadCallable'.

; Test based on IR generated from the following HLSL:
; struct Param { float f; };
; [shader("callable")]
; void CallableProto(inout Param p) { p.f += 1.0; }
;
; export void BadCallable(inout float f) { f += 1.0; }

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @BadCallable(float* noalias nocapture dereferenceable(4) %f) #0 {
  ret void
}

attributes #0 = { nounwind }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 7}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{void (float*)* @BadCallable, !"BadCallable", null, null, !5}
!5 = !{i32 8, i32 12, i32 6, i32 4, i32 5, !6}
!6 = !{i32 0}
