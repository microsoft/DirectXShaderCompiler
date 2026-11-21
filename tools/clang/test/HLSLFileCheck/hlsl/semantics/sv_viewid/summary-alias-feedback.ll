; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 5, i32 10, i32 0, i32 0]}
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
@g = internal global float 0.0
define float @feedback(float %x, float* %p, float* %q) noinline {
 %local = alloca float
 %v = load float, float* %p
 store float %v, float* %local
 store float %x, float* %q
 %r = load float, float* %local
 ret float %r
}
define float @privateLocal(float %x) noinline {
 %p = alloca float
 store float %x, float* %p
 %r = load float, float* %p
 ret float %r
}
define void @main() {
 %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
 %y = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
 %a = alloca float
 store float 0.0, float* %a
 %r0 = call float @feedback(float %x, float* %a, float* %a)
 %r1 = call float @feedback(float %y, float* @g, float* @g)
 %r2 = call float @privateLocal(float %x)
 %r3 = call float @privateLocal(float %y)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r0)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %r1)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %r2)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %r3)
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
