; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; A nested select supplies a call actual or a PHI condition. Equality of the
; inner actuals removes x; distinct inner actuals retain x in outputs 2 and 3.
; Retaining the inner inequality when adding the outer guard is conservative.
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 12, i32 0, i32 0, i32 0]}
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
define float @choose(float %a, float %b, float %condition) noinline {
 %c = fcmp ogt float %condition, 0.0
 %v = select i1 %c, float %a, float %b
 ret float %v
}
define float @nested(float %a, float %b, float %x) noinline {
 %c = fcmp ogt float %x, 0.0
 %s = select i1 %c, float %a, float %b
 %v = call float @choose(float 7.0, float 11.0, float %s)
 ret float %v
}
define float @nestedphi(float %a, float %b, float %x) noinline {
entry:
 %c = fcmp ogt float %x, 0.0
 %s = select i1 %c, float %a, float %b
 %d = fcmp ogt float %s, 0.0
 br i1 %d, label %yes, label %no
yes:
 br label %merge
no:
 br label %merge
merge:
 %v = phi float [7.0, %yes], [11.0, %no]
 ret float %v
}
define void @main() {
 %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
 %a = call float @nested(float 1.0, float 1.0, float %x)
 %b = call float @nestedphi(float 1.0, float 1.0, float %x)
 %c = call float @nested(float 1.0, float -1.0, float %x)
 %d = call float @nestedphi(float 1.0, float -1.0, float %x)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %a)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %b)
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %c)
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
