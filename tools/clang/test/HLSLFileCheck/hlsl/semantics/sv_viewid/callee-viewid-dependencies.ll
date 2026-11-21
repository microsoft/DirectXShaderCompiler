; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[7 x i32] [i32 4, i32 4, i32 1, i32 2, i32 4, i32 0, i32 0]}
;
; The ViewID read exists only in the callee. Call operands alone are insufficient.
; Also cover conditional constant stores and globals shared with nested calls.
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
@g = internal global float 0.0
@unused = internal global float 0.0

define void @view(float* %dst) noinline {
  %v = call i32 @dx.op.viewID.i32(i32 138)
  %c = icmp ne i32 %v, 0
  br i1 %c, label %write, label %exit
write:
  store float 3.0, float* %dst
  br label %exit
exit:
  ret void
}
define void @conditional(float %v, float* %dst) noinline {
  %c = fcmp ogt float %v, 0.0
  br i1 %c, label %write, label %exit
write:
  store float 5.0, float* %dst
  br label %exit
exit:
  ret void
}
define void @globalWrite(float %v) noinline {
  store float %v, float* @g
  ret void
}
define void @forward(float %v) noinline {
  call void @globalWrite(float %v)
  ret void
}
define void @dead(float %v) noinline {
  store float %v, float* @g
  ret void
}
define void @main() {
  %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %y = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %z = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)
  %a = alloca float
  %b = alloca float
  store float 0.0, float* %a
  store float 0.0, float* %b
  store float %z, float* @unused
  call void @view(float* %a)
  call void @conditional(float %x, float* %b)
  call void @forward(float %y)
  %r0 = load float, float* %a
  %r1 = load float, float* %b
  %r2 = load float, float* @g
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %r1)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %r2)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 11.0)
  ret void
}
declare i32 @dx.op.viewID.i32(i32) nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) nounwind readnone
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) nounwind
!dx.version = !{!0}
!dx.valver = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}
!0 = !{i32 1, i32 1}
!1 = !{!"ps", i32 6, i32 1}
!2 = !{void ()* @main, !"main", !3, null, !9}
!3 = !{!4, !5, null}
!4 = !{!6}
!5 = !{!7}
!6 = !{i32 0, !"INPUT", i8 9, i8 0, !8, i8 2, i32 1, i8 4, i32 0, i8 0, null}
!7 = !{i32 0, !"SV_Target", i8 9, i8 16, !8, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!8 = !{i32 0}
!9 = !{i32 0, i64 2097152}
