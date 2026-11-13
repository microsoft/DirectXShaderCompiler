; RUN: %dxopt %s -indvars -S | FileCheck %s

; ScalarEvolution::createNodeForPHI used to apply the no-wrap flags of an IV
; increment to the post-increment add recurrence unconditionally. That add
; recurrence is also the SCEV of any other instruction computing the same
; value, even one that executes on iterations where the increment does not.
; SimplifyIndvar then used those flags to mark such an instruction nsw.
; Partial backport of PR27315:
; https://github.com/llvm/llvm-project/commit/7e4a64167d4d2e7b0b680fae1706182223047af1

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; %inc_i is nsw because it only executes below the exit guard, where
; %i < INT32_MAX. With %c true, %inc_u computes INT32_MAX + 1 in the header on
; the second iteration, so it must not become nsw.
define i32 @guarded_latch_inc_flags(i1 %c) {
; CHECK-LABEL: @guarded_latch_inc_flags(
; CHECK:       loop:
; CHECK-NEXT:    %i = phi i32 [ 2147483646, %entry ], [ %inc_i, %latch ]
; CHECK-NEXT:    %inc_u = add nuw i32 %i, 1
; CHECK-NEXT:    %cond = icmp sgt i32 %i, 2147483646
; CHECK-NEXT:    br i1 %cond, label %exit, label %latch
; CHECK:       latch:
; CHECK-NEXT:    %inc_i = add nuw nsw i32 %i, 1
; CHECK-NEXT:    br i1 %c, label %loop, label %exit
entry:
  br label %loop

loop:
  %i = phi i32 [ 2147483646, %entry ], [ %inc_i, %latch ]
  %inc_u = add i32 %i, 1
  %cond = icmp sgt i32 %i, 2147483646
  br i1 %cond, label %exit, label %latch

latch:
  %inc_i = add nsw i32 %i, 1
  br i1 %c, label %loop, label %exit

exit:
  %res = phi i32 [ %inc_u, %loop ], [ %inc_u, %latch ]
  ret i32 %res
}
