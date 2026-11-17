; RUN: %dxopt %s -hlsl-passes-resume -simplify-inst -S | FileCheck %s --check-prefix=FOLD
; RUN: %dxopt %s -hlsl-passes-resume -indvars -S | FileCheck %s --check-prefix=INDVARS

; Check that DXIL dot products are not constant folded when every product is
; -0.0, so that the sign of the zero is the one the driver computes, and that
; IndVarSimplify keeps the bound of a loop whose early exit checks the bits of
; such a dot product.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; FOLD-LABEL: define float @dot4_neg_zero_f32()
; FOLD-NEXT: %r = call float @dx.op.dot4.f32(
define float @dot4_neg_zero_f32() {
  %r = call float @dx.op.dot4.f32(i32 56, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float -1.000000e+00, float -1.000000e+00, float -1.000000e+00, float -1.000000e+00)
  ret float %r
}

; FOLD-LABEL: define float @dot2_mixed_zero_f32()
; FOLD-NEXT: ret float 0.000000e+00
define float @dot2_mixed_zero_f32() {
  %r = call float @dx.op.dot2.f32(i32 54, float 0.000000e+00, float 0.000000e+00, float -1.000000e+00, float 1.000000e+00)
  ret float %r
}

; INDVARS-LABEL: define void @indvars_dot4_neg_zero_loop(
; INDVARS: icmp ult i32 %i.next, 128
define void @indvars_dot4_neg_zero_loop([128 x i32] addrspace(3)* %wg) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %f = phi float [ 0.000000e+00, %entry ], [ %f.next, %latch ]
  %z = fsub float 9.000000e+01, %f
  %d = call float @dx.op.dot4.f32(i32 56, float %z, float %z, float %z, float %z, float -1.000000e+00, float -1.000000e+00, float -1.000000e+00, float -1.000000e+00)
  %bits = bitcast float %d to i32
  %early = icmp eq i32 %bits, 0
  br i1 %early, label %exit, label %latch

latch:
  %gep = getelementptr inbounds [128 x i32], [128 x i32] addrspace(3)* %wg, i32 0, i32 %i
  store i32 %i, i32 addrspace(3)* %gep, align 4
  %f.next = fadd float %f, 1.000000e+00
  %i.next = add nuw nsw i32 %i, 1
  %cmp = icmp ult i32 %i.next, 128
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

define void @main() {
  ret void
}

declare float @dx.op.dot2.f32(i32, float, float, float, float) #0
declare float @dx.op.dot4.f32(i32, float, float, float, float, float, float, float, float) #0

attributes #0 = { nounwind readnone }

!pauseresume = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.entryPoints = !{!4}

!0 = !{!"hlsl-dxilemit", !"hlsl-dxilload"}
!1 = !{i32 1, i32 2}
!2 = !{i32 1, i32 2}
!3 = !{!"cs", i32 6, i32 2}
!4 = !{void ()* @main, !"main", null, null, !5}
!5 = !{i32 4, !6}
!6 = !{i32 1, i32 1, i32 1}
