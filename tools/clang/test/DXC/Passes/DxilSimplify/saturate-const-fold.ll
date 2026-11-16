; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-expand-trig-intrinsics -simplify-inst -S | FileCheck %s --check-prefix=FOLD
; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-expand-trig-intrinsics -indvars -S | FileCheck %s --check-prefix=INDVARS

; Verify that DxilConstantFolding folds DXIL Saturate in the spec's operand
; order, min(1.0, max(0.0, x)), where FMax returns its first operand on ties,
; so Saturate(-0.0) folds to +0.0.
;
; Also check that ScalarEvolution's ComputeExitCountExhaustively (via
; IndVarSimplify) does not delete the loop exit bound when an early break
; checks for asuint(saturate(-0.0)) == 0x80000000.
;
; The preceding -hlsl-dxil-expand-trig-intrinsics pass forces DxilModule
; creation from the module metadata so CanConstantFoldCallTo accepts dx.op.*.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

@wg = external addrspace(3) global [128 x i32], align 4

declare half @dx.op.unary.f16(i32, half) #0
declare float @dx.op.unary.f32(i32, float) #0
declare double @dx.op.unary.f64(i32, double) #0

; FOLD-LABEL: define void @saturate_neg_zero(
; FOLD-NOT: dx.op.unary
; FOLD: store half 0xH0000, half* %hout
; FOLD: store float 0.000000e+00, float* %fout
; FOLD: store double 0.000000e+00, double* %dout
define void @saturate_neg_zero(half* %hout, float* %fout, double* %dout) {
  %h = call half @dx.op.unary.f16(i32 7, half 0xH8000)
  store half %h, half* %hout
  %f = call float @dx.op.unary.f32(i32 7, float -0.000000e+00)
  store float %f, float* %fout
  %d = call double @dx.op.unary.f64(i32 7, double -0.000000e+00)
  store double %d, double* %dout
  ret void
}

; FOLD-LABEL: define void @saturate_fold_normal_and_pos_zero(
; FOLD-NOT: dx.op.unary
; FOLD: store float 0.000000e+00, float* %out
; FOLD: store float 0.000000e+00, float* %out
; FOLD: store float 5.000000e-01, float* %out
; FOLD: store float 1.000000e+00, float* %out
define void @saturate_fold_normal_and_pos_zero(float* %out) {
  %pzero = call float @dx.op.unary.f32(i32 7, float 0.000000e+00)
  store float %pzero, float* %out
  %neg = call float @dx.op.unary.f32(i32 7, float -5.000000e-01)
  store float %neg, float* %out
  %mid = call float @dx.op.unary.f32(i32 7, float 5.000000e-01)
  store float %mid, float* %out
  %hi = call float @dx.op.unary.f32(i32 7, float 2.000000e+00)
  store float %hi, float* %out
  ret void
}

; INDVARS-LABEL: define i32 @saturate_loop_bound(
; INDVARS: latch:
; INDVARS: %i.next = add nuw nsw i32 %i, 1
; INDVARS: %bound = icmp ult i32 %i.next, 128
; INDVARS: br i1 %bound, label %loop, label %exit
define i32 @saturate_loop_bound(i32 %stop) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  %f = phi float [ 0.000000e+00, %entry ], [ %f.next, %latch ]
  %sub = fsub float 8.950000e+01, %f
  %z = fmul float %sub, 0.000000e+00
  %sat = call float @dx.op.unary.f32(i32 7, float %z)
  %bits = bitcast float %sat to i32
  %is.negzero = icmp eq i32 %bits, -2147483648
  br i1 %is.negzero, label %exit, label %check.stop

check.stop:
  %is.stop = icmp eq i32 %i, %stop
  br i1 %is.stop, label %exit, label %latch

latch:
  %gep = getelementptr [128 x i32], [128 x i32] addrspace(3)* @wg, i32 0, i32 %i
  %val = load i32, i32 addrspace(3)* %gep, align 4
  %acc.next = add i32 %acc, %val
  %f.next = fadd float %f, 1.000000e+00
  %i.next = add i32 %i, 1
  %bound = icmp ult i32 %i.next, 128
  br i1 %bound, label %loop, label %exit

exit:
  %res = phi i32 [ %acc, %loop ], [ %acc, %check.stop ], [ %acc.next, %latch ]
  ret i32 %res
}

attributes #0 = { nounwind readnone }

!dx.version = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}

!0 = !{i32 1, i32 0}
!1 = !{!"cs", i32 6, i32 0}
!2 = !{void (half*, float*, double*)* @saturate_neg_zero, !"saturate_neg_zero", null, null, !3}
!3 = !{i32 4, !4}
!4 = !{i32 1, i32 1, i32 1}
