; RUN: %dxopt %s -viewid-state -hlsl-dxilemit -S | FileCheck %s
; CHECK: !dx.viewIdState = !{![[STATE:[0-9]+]]}
; CHECK: ![[STATE]] = !{[6 x i32] [i32 4, i32 4, i32 1, i32 0, i32 0, i32 0]}
; Physical block order is the reverse of the reachable def-use/copy chain.
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i64:64-f16:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
define void @main() {
entry:
 %x = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
 %a0 = alloca float
 %c0 = bitcast float* %a0 to i8*
 %a1 = alloca float
 %c1 = bitcast float* %a1 to i8*
 %a2 = alloca float
 %c2 = bitcast float* %a2 to i8*
 %a3 = alloca float
 %c3 = bitcast float* %a3 to i8*
 %a4 = alloca float
 %c4 = bitcast float* %a4 to i8*
 store float %x, float* %a0
 br label %b0
b4:
 %r = load float, float* %a4
 call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r)
 ret void
b3:
 call void @llvm.memcpy.p0i8.p0i8.i32(i8* %c4, i8* %c3, i32 4, i32 4, i1 false)
 br label %b4
b2:
 call void @llvm.memcpy.p0i8.p0i8.i32(i8* %c3, i8* %c2, i32 4, i32 4, i1 false)
 br label %b3
b1:
 call void @llvm.memcpy.p0i8.p0i8.i32(i8* %c2, i8* %c1, i32 4, i32 4, i1 false)
 br label %b2
b0:
 call void @llvm.memcpy.p0i8.p0i8.i32(i8* %c1, i8* %c0, i32 4, i32 4, i1 false)
 br label %b1
}
declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i32, i1) nounwind
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
