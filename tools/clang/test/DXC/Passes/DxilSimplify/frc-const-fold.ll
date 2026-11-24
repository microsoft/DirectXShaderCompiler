; RUN: %dxopt %s -hlsl-passes-resume -simplify-inst -S | FileCheck %s --check-prefixes=CHECK,SIMPLIFY
; RUN: %dxopt %s -hlsl-passes-resume -instcombine -S | FileCheck %s --check-prefixes=CHECK,SIMPLIFY
; RUN: %dxopt %s -hlsl-passes-resume -indvars -S | FileCheck %s --check-prefix=INDVARS

; Verify constant folding of DXIL Frc (OP::OpCode::Frc = 22) matches HLSL
; frac(x) = x - floor(x) in [0, 1), leaves fp32 denormals unfolded, and does
; not miscompute loop trip counts in IndVarSimplify/SCEV.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; CHECK-LABEL: define float @frc_neg_1_25()
; SIMPLIFY-NEXT: ret float 7.500000e-01
define float @frc_neg_1_25() {
  %r = call float @dx.op.unary.f32(i32 22, float -1.250000e+00)
  ret float %r
}

; CHECK-LABEL: define half @frc_half_neg_1_25()
; SIMPLIFY-NEXT: ret half 0xH3A00
define half @frc_half_neg_1_25() {
  %r = call half @dx.op.unary.f16(i32 22, half 0xHBD00)
  ret half %r
}

; CHECK-LABEL: define double @frc_double_neg_1_25()
; SIMPLIFY-NEXT: ret double 7.500000e-01
define double @frc_double_neg_1_25() {
  %r = call double @dx.op.unary.f64(i32 22, double -1.250000e+00)
  ret double %r
}

; CHECK-LABEL: define float @frc_pos_1_25()
; SIMPLIFY-NEXT: ret float 2.500000e-01
define float @frc_pos_1_25() {
  %r = call float @dx.op.unary.f32(i32 22, float 1.250000e+00)
  ret float %r
}

; Negative integers must fold to +0.0 (not -0.0).
; CHECK-LABEL: define float @frc_neg_int()
; SIMPLIFY-NEXT: ret float 0.000000e+00
define float @frc_neg_int() {
  %r = call float @dx.op.unary.f32(i32 22, float -2.000000e+00)
  ret float %r
}

; Signed zero must fold to +0.0.
; CHECK-LABEL: define float @frc_neg_zero()
; SIMPLIFY-NEXT: ret float 0.000000e+00
define float @frc_neg_zero() {
  %r = call float @dx.op.unary.f32(i32 22, float -0.000000e+00)
  ret float %r
}

; fp32 denormals must not be folded (via IsValidOp).
; CHECK-LABEL: define float @frc_neg_denorm()
; SIMPLIFY-NEXT: %r = call float @dx.op.unary.f32(i32 22, float 0xB6A0000000000000)
; SIMPLIFY-NEXT: ret float %r
define float @frc_neg_denorm() {
  %r = call float @dx.op.unary.f32(i32 22, float 0xB6A0000000000000)
  ret float %r
}

; Small negative normal inputs round up to 1.0 in the result type, outside
; Frc's [0, 1) range, so they must not be folded.
; CHECK-LABEL: define float @frc_small_neg_float()
; SIMPLIFY-NEXT: %r = call float @dx.op.unary.f32(i32 22, float 0xBE10000000000000)
; SIMPLIFY-NEXT: ret float %r
define float @frc_small_neg_float() {
  %r = call float @dx.op.unary.f32(i32 22, float 0xBE10000000000000)
  ret float %r
}

; CHECK-LABEL: define half @frc_small_neg_half()
; SIMPLIFY-NEXT: %r = call half @dx.op.unary.f16(i32 22, half 0xH8800)
; SIMPLIFY-NEXT: ret half %r
define half @frc_small_neg_half() {
  %r = call half @dx.op.unary.f16(i32 22, half 0xH8800)
  ret half %r
}

; CHECK-LABEL: define double @frc_small_neg_double()
; SIMPLIFY-NEXT: %r = call double @dx.op.unary.f64(i32 22, double 0xBC30000000000000)
; SIMPLIFY-NEXT: ret double %r
define double @frc_small_neg_double() {
  %r = call double @dx.op.unary.f64(i32 22, double 0xBC30000000000000)
  ret double %r
}

; At the boundary, -2^-24 folds to 1 - 2^-24, the largest float below 1.0.
; CHECK-LABEL: define float @frc_neg_boundary_float()
; SIMPLIFY-NEXT: ret float 0x3FEFFFFFE0000000
define float @frc_neg_boundary_float() {
  %r = call float @dx.op.unary.f32(i32 22, float 0xBE70000000000000)
  ret float %r
}

; IndVarSimplify/SCEV must preserve the loop bound icmp ult i32 %i.next, 128
; when Frc is called on negative evolving floats. Previously, Frc(-0.755)
; folded to 0.755 > 0.75 around iteration 91, causing IndVarSimplify to fold
; the 128 bound away.
; INDVARS-LABEL: define void @indvars_frc_neg_loop(
; INDVARS: icmp ult i32 %i.next, 128
define void @indvars_frc_neg_loop([128 x i32] addrspace(3)* %wg) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %f = phi float [ 0xBFD3333340000000, %entry ], [ %f.next, %latch ]
  %frc = call float @dx.op.unary.f32(i32 22, float %f)
  %early = fcmp ogt float %frc, 7.500000e-01
  br i1 %early, label %exit, label %latch

latch:
  %gep = getelementptr inbounds [128 x i32], [128 x i32] addrspace(3)* %wg, i32 0, i32 %i
  store i32 %i, i32 addrspace(3)* %gep, align 4
  %f.next = fsub float %f, 0x3F747AE140000000
  %i.next = add nuw nsw i32 %i, 1
  %cmp = icmp ult i32 %i.next, 128
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

define void @main() {
  ret void
}

declare half @dx.op.unary.f16(i32, half) #0
declare float @dx.op.unary.f32(i32, float) #0
declare double @dx.op.unary.f64(i32, double) #0

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
