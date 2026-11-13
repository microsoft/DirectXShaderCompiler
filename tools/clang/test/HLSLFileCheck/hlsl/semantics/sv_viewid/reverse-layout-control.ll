; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; The predicate is last in physical order. Its growth must wake the retained
; calls and PHI selection even though they are not ordinary users of the branch.
; emit has an output effect with an empty dependency set until caller control
; is applied. That effect must survive summary construction.
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 7, i32 0, i32 0, i32 0]}

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @write(float* %p) noinline {
 store float 5.0, float* %p
 ret void
}

define void @emit() noinline {
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 7.0)
 ret void
}

define void @main() {
entry:
 %p = alloca float
 store float 0.0, float* %p
 %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
 br label %test
write:
 call void @write(float* %p)
 call void @emit()
 br label %merge
merge:
 %s = phi float [5.0, %write], [7.0, %test]
 %v = load float, float* %p
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %v)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %s)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 11.0)
 ret void
test:
 %c = fcmp ogt float %x, 0.0
 br i1 %c, label %write, label %merge
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
