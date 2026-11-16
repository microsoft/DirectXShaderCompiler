; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-expand-trig-intrinsics -simplify-inst -S | FileCheck %s --check-prefix=FOLD
; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-expand-trig-intrinsics -indvars -S | FileCheck %s --check-prefix=INDVARS

; Regression test for DxilConstantFolding of FMin (opcode 36) and FMax (opcode 35).
;
; DXIL leaves tie-breaking between equal finite values with distinct bit
; patterns (+0.0 vs -0.0) implementation-defined ("a or b"). Constant folding
; must bail out on mixed-sign zero ties while still folding bitwise-equal
; zeros and normal finite operands. Without the fix, ScalarEvolution /
; IndVarSimplify can misfold bitcast exit checks on (89.5 - f) * 0.0 and
; delete the real loop bound `icmp ult ..., 128`.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; --- Mixed-sign zero ties: must NOT fold ---

; FOLD-LABEL: define void @fmin_fmax_mixed_zero_f32()
; FOLD: call float @dx.op.binary.f32(i32 36, float 0.000000e+00, float -0.000000e+00)
; FOLD: call float @dx.op.binary.f32(i32 36, float -0.000000e+00, float 0.000000e+00)
; FOLD: call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float -0.000000e+00)
; FOLD: call float @dx.op.binary.f32(i32 35, float -0.000000e+00, float 0.000000e+00)
define void @fmin_fmax_mixed_zero_f32() {
  %min_pn = call float @dx.op.binary.f32(i32 36, float 0.0, float -0.0)
  %min_np = call float @dx.op.binary.f32(i32 36, float -0.0, float 0.0)
  %max_pn = call float @dx.op.binary.f32(i32 35, float 0.0, float -0.0)
  %max_np = call float @dx.op.binary.f32(i32 35, float -0.0, float 0.0)
  %r0 = fadd float %min_pn, %min_np
  %r1 = fadd float %max_pn, %max_np
  %r = fadd float %r0, %r1
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %r)
  ret void
}

; --- Bitwise-equal zeros and normal finite values: MUST STILL fold ---

; FOLD-LABEL: define void @fmin_fmax_valid_fold_f32()
; FOLD-NOT: dx.op.binary
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 1, i8 0, float -0.000000e+00)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 2, i8 0, float 0.000000e+00)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 3, i8 0, float -0.000000e+00)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 4, i8 0, float 2.500000e-01)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 5, i8 0, float 5.000000e-01)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 6, i8 0, float -0.000000e+00)
; FOLD: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 7, i8 0, float 0.000000e+00)
define void @fmin_fmax_valid_fold_f32() {
  %min_pp = call float @dx.op.binary.f32(i32 36, float 0.0, float 0.0)
  %min_nn = call float @dx.op.binary.f32(i32 36, float -0.0, float -0.0)
  %max_pp = call float @dx.op.binary.f32(i32 35, float 0.0, float 0.0)
  %max_nn = call float @dx.op.binary.f32(i32 35, float -0.0, float -0.0)
  %min_norm = call float @dx.op.binary.f32(i32 36, float 0.5, float 0.25)
  %max_norm = call float @dx.op.binary.f32(i32 35, float 0.5, float 0.25)
  %min_n0_1 = call float @dx.op.binary.f32(i32 36, float -0.0, float 1.0)
  %max_m1_p0 = call float @dx.op.binary.f32(i32 35, float -1.0, float 0.0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %min_pp)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 1, i8 0, float %min_nn)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 2, i8 0, float %max_pp)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 3, i8 0, float %max_nn)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 4, i8 0, float %min_norm)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 5, i8 0, float %max_norm)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 6, i8 0, float %min_n0_1)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 7, i8 0, float %max_m1_p0)
  ret void
}

; --- IndVarSimplify / SCEV loop bound preservation ---

; INDVARS-LABEL: define i32 @indvars_fmin_signed_zero_preserves_bound(
; INDVARS: icmp ult i32 {{.*}}, 128
define i32 @indvars_fmin_signed_zero_preserves_bound(i32 %stop) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %f = phi float [ 0.0, %entry ], [ %f.next, %latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  %diff = fsub float 8.950000e+01, %f
  %z = fmul float %diff, 0.0
  %m = call float @dx.op.binary.f32(i32 36, float 0.0, float %z)
  %m.bits = bitcast float %m to i32
  %z.bits = bitcast float %z to i32
  %same = icmp eq i32 %m.bits, %z.bits
  %not_stop = icmp ne i32 %i, %stop
  %cont = and i1 %same, %not_stop
  br i1 %cont, label %latch, label %exit

latch:
  %acc.next = add i32 %acc, %i
  %f.next = fadd float %f, 1.0
  %i.next = add nuw nsw i32 %i, 1
  %bound = icmp ult i32 %i.next, 128
  br i1 %bound, label %loop, label %exit

exit:
  %res = phi i32 [ %acc, %loop ], [ %acc.next, %latch ]
  ret i32 %res
}

declare float @dx.op.binary.f32(i32, float, float) #0
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!dx.version = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}

!0 = !{i32 1, i32 2}
!1 = !{!"cs", i32 6, i32 2}
!2 = !{void ()* @fmin_fmax_mixed_zero_f32, !"fmin_fmax_mixed_zero_f32", null, null, !3}
!3 = !{i32 4, !4}
!4 = !{i32 1, i32 1, i32 1}
