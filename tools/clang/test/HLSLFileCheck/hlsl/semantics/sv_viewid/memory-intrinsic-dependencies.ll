; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 5, i32 1, i32 0, i32 0]}
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
define void @copy(float* %src, float* %dst) noinline {
  %s = bitcast float* %src to i8*
  %d = bitcast float* %dst to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %d, i8* %s, i32 4, i32 4, i1 false)
  ret void
}
define void @atomic(i32 %v, i32* %dst) noinline {
  %old = atomicrmw add i32* %dst, i32 %v monotonic
  ret void
}
define void @main() {
  %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %y = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %a = alloca float
  %b = alloca float
  %c = alloca float
  %atom = alloca i32
  store float %x, float* %a
  call void @copy(float* %a, float* %b)
  %raw = bitcast float* %c to i8*
  call void @llvm.memset.p0i8.i32(i8* %raw, i8 0, i32 4, i32 4, i1 false)
  %xi = fptoui float %x to i32
  %yi = fptoui float %y to i32
  store i32 %xi, i32* %atom
  call void @atomic(i32 %yi, i32* %atom)
  %ri = load i32, i32* %atom
  %r0 = uitofp i32 %ri to float
  %r1 = load float, float* %c
  %r2 = load float, float* %b
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %r1)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %r2)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 11.0)
  ret void
}
declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i32, i1) nounwind
declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i32, i1) nounwind
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
