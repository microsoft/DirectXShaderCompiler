; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 1, i32 2, i32 0, i32 8]}
;
; Ordinary readnone declarations are valid inputs to this dependency pass.
; Their constant arguments must not be decoded as DXIL opcodes. The lifetime
; marker ends storage after its last use and must not act like an ordinary store.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

declare float @pure(i32, float) nounwind readnone
declare void @llvm.lifetime.end(i64, i8* nocapture) nounwind

define void @main() {
entry:
 %p = alloca float
 store float 7.0, float* %p
 %constant = load float, float* %p
 %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
 %y = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
 %z = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)
 %w = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)
 %a = call float @pure(i32 2, float %x)
 %b = call float @pure(i32 3, float %y)
 %d = call float @pure(i32 170, float %w)
 %condition = fcmp ogt float %z, 0.0
 br i1 %condition, label %end, label %output
end:
 %bytes = bitcast float* %p to i8*
 call void @llvm.lifetime.end(i64 3, i8* %bytes)
 br label %output
output:
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %a)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %b)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %constant)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %d)
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
