; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-expand-trig-intrinsics -simplify-inst -S | FileCheck %s --check-prefix=SIMPLIFY
; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-expand-trig-intrinsics -indvars -S | FileCheck %s --check-prefix=INDVARS

; Verify DxilConstantFolding does not fold 32-bit float denormal operands,
; whose runtime behavior depends on the target and fp32-denorm-mode, while
; continuing to fold zero/normal fp32, fp16 denormals (preserved in SM 6.2+),
; and fp64 denormals (required to fold transient literal-float intrinsics).
;
; Also verify ScalarEvolution + IndVarSimplify preserves integer loop bounds
; when an early exit depends on a DXIL fp32 intrinsic over denormal inputs.
;
; -hlsl-dxil-expand-trig-intrinsics triggers DxilModule initialization from DXIL
; metadata without modifying the non-trig IR under test.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; --- fp32 denormals must NOT be folded ---

; SIMPLIFY-LABEL: define float @fp32_denorm_round_pi_min_pos()
; SIMPLIFY: %r = call float @dx.op.unary.f32(i32 28, float 0x36A0000000000000)
; SIMPLIFY: ret float %r
define float @fp32_denorm_round_pi_min_pos() {
  %r = call float @dx.op.unary.f32(i32 28, float 0x36A0000000000000)
  ret float %r
}

; SIMPLIFY-LABEL: define float @fp32_denorm_round_ni_neg()
; SIMPLIFY: %r = call float @dx.op.unary.f32(i32 27, float 0xB6A0000000000000)
; SIMPLIFY: ret float %r
define float @fp32_denorm_round_ni_neg() {
  %r = call float @dx.op.unary.f32(i32 27, float 0xB6A0000000000000)
  ret float %r
}

; SIMPLIFY-LABEL: define float @fp32_denorm_sqrt_pos()
; SIMPLIFY: %r = call float @dx.op.unary.f32(i32 24, float 0x36A0000000000000)
; SIMPLIFY: ret float %r
define float @fp32_denorm_sqrt_pos() {
  %r = call float @dx.op.unary.f32(i32 24, float 0x36A0000000000000)
  ret float %r
}

; --- Boundary sanity checks that MUST still fold ---

; SIMPLIFY-LABEL: define float @fp32_pos_zero_round_pi()
; SIMPLIFY-NOT: dx.op.unary.f32
; SIMPLIFY: ret float 0.000000e+00
define float @fp32_pos_zero_round_pi() {
  %r = call float @dx.op.unary.f32(i32 28, float 0.000000e+00)
  ret float %r
}

; SIMPLIFY-LABEL: define float @fp32_neg_zero_round_ni()
; SIMPLIFY-NOT: dx.op.unary.f32
; SIMPLIFY: ret float -0.000000e+00
define float @fp32_neg_zero_round_ni() {
  %r = call float @dx.op.unary.f32(i32 27, float -0.000000e+00)
  ret float %r
}

; SIMPLIFY-LABEL: define float @fp32_flt_min_round_pi()
; SIMPLIFY-NOT: dx.op.unary.f32
; SIMPLIFY: ret float 1.000000e+00
define float @fp32_flt_min_round_pi() {
  %r = call float @dx.op.unary.f32(i32 28, float 0x3810000000000000)
  ret float %r
}

; SIMPLIFY-LABEL: define half @fp16_denorm_pos_round_pi()
; SIMPLIFY-NOT: dx.op.unary.f16
; SIMPLIFY: ret half 0xH3C00
define half @fp16_denorm_pos_round_pi() {
  %r = call half @dx.op.unary.f16(i32 28, half 0xH0001)
  ret half %r
}

; SIMPLIFY-LABEL: define double @fp64_denorm_sqrt()
; SIMPLIFY-NOT: dx.op.unary.f64
; SIMPLIFY: ret double 0x1E60000000000000
define double @fp64_denorm_sqrt() {
  %r = call double @dx.op.unary.f64(i32 24, double 0x0000000000000001)
  ret double %r
}

; --- SCEV + IndVarSimplify must preserve loop bound exit ---

; INDVARS-LABEL: define i32 @loop_round_pi_denorm_exit(i32 %x)
; INDVARS: latch:
; INDVARS: %bound = icmp ult i32 %i.next, 128
; INDVARS: br i1 %bound, label %loop, label %exit
define i32 @loop_round_pi_denorm_exit(i32 %x) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  %shifted = lshr i32 %i, 6
  %f = bitcast i32 %shifted to float
  %ceil = call float @dx.op.unary.f32(i32 28, float %f)
  %early = fcmp oeq float %ceil, 1.000000e+00
  br i1 %early, label %exit, label %latch

latch:
  %acc.next = add i32 %acc, %x
  %i.next = add i32 %i, 1
  %bound = icmp ult i32 %i.next, 128
  br i1 %bound, label %loop, label %exit

exit:
  %res = phi i32 [ %acc, %loop ], [ %acc.next, %latch ]
  ret i32 %res
}

define void @main() {
  ret void
}

declare half @dx.op.unary.f16(i32, half) #0
declare float @dx.op.unary.f32(i32, float) #0
declare double @dx.op.unary.f64(i32, double) #0

attributes #0 = { nounwind readnone }

!dx.version = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}

!0 = !{i32 1, i32 2}
!1 = !{!"cs", i32 6, i32 2}
!2 = !{void ()* @main, !"main", null, null, !3}
!3 = !{i32 4, !4}
!4 = !{i32 1, i32 1, i32 1}
