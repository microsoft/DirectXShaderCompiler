; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 1, i32 2, i32 1, i32 0]}
;
; Separate shared-callee contexts, nested pointer reads, pointer select/PHI
; cycles, casts, GEP address dependence, and an unrelated dead function.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @copy(float* %src, float* %dst) noinline {
  %v = load float, float* %src
  store float %v, float* %dst
  ret void
}
define void @nested(float* %src, float* %dst) noinline {
  call void @copy(float* %src, float* %dst)
  ret void
}
define void @dead(float %v, float* %dst) noinline {
  store float %v, float* %dst
  ret void
}
define void @main() {
entry:
  %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %y = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %z = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)
  %w = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)
  %a = alloca [2 x float]
  %b = alloca float
  %out0 = alloca float
  %out1 = alloca float
  %unrelated = alloca float
  store float %w, float* %unrelated
  %idx = fptoui float %z to i32
  %index = and i32 %idx, 1
  %p = getelementptr [2 x float], [2 x float]* %a, i32 0, i32 %index
  %p0 = getelementptr [2 x float], [2 x float]* %a, i32 0, i32 0
  %p1 = getelementptr [2 x float], [2 x float]* %a, i32 0, i32 1
  store float %x, float* %p
  store float %y, float* %b
  %choose = fcmp ogt float %z, 0.0
  %selected = select i1 %choose, float* %p0, float* %p1
  br label %loop
loop:
  %alias = phi float* [ %selected, %entry ], [ %next, %loop ]
  %bytes = bitcast float* %alias to i8*
  %next = bitcast i8* %bytes to float*
  br i1 undef, label %exit, label %loop
exit:
  call void @nested(float* %next, float* %out0)
  call void @nested(float* %b, float* %out1)
  call void @nested(float* %b, float* %b)
  %r0 = load float, float* %out0
  %r1 = load float, float* %out1
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %r1)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 7.0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 11.0)
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
