; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 1, i32 0, i32 0, i32 0]}
; Copies run from the end toward the start, so input reaches the final
; slot over multiple loop iterations. Retained helper memory reads must be
; revisited after later calls write their sources. The source pointer also
; has a loop-carried PHI/GEP cycle.
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
define void @copy(float* %dst, float* %src) noinline {
 %v = load float, float* %src
 store float %v, float* %dst
 ret void
}
define void @main() {
entry:
 %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
 %a0 = alloca float
 store float %x, float* %a0
 %a1 = alloca float
 store float 0.0, float* %a1
 %a2 = alloca float
 store float 0.0, float* %a2
 %a3 = alloca float
 store float 0.0, float* %a3
 %a4 = alloca float
 store float 0.0, float* %a4
 br label %loop
loop:
 %p = phi float* [%a0, %entry], [%next, %loop]
 %next = getelementptr float, float* %p, i32 0
 call void @copy(float* %a4, float* %a3)
 call void @copy(float* %a3, float* %a2)
 call void @copy(float* %a2, float* %a1)
 call void @copy(float* %a1, float* %p)
 br i1 undef, label %loop, label %exit
exit:
 %r = load float, float* %a4
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r)
 ret void
}
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) nounwind readnone
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) nounwind
!dx.version = !{!0}
!dx.valver = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}
!0 = !{i32 1, i32 1}
!1 = !{!"ps", i32 6, i32 1}
!2 = !{void ()* @main, !"main", !3, null, null}
!3 = !{!4, !5, null}
!4 = !{!6}
!5 = !{!7}
!6 = !{i32 0, !"INPUT", i8 9, i8 0, !8, i8 2, i32 1, i8 4, i32 0, i8 0, null}
!7 = !{i32 0, !"SV_Target", i8 9, i8 16, !8, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!8 = !{i32 0}
