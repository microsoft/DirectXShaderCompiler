; RUN: %dxopt %s -indvars -S | FileCheck %s

; Regression test for the backport of
; https://github.com/llvm/llvm-project/commit/2ca8a3f2132ec7d46151b4553ee166c4e635fd70
; (PR23527, D109789).
; ScalarEvolution::getGEPExpr must not blindly transfer the inbounds flag from
; a GEP instruction to its SCEV expression, because the GEP may be guarded by
; control flow and its no-wrap flags can pollute a globally uniqued
; SCEVAddRecExpr shared with the loop's induction variable.
;
; Here, %gep in the guarded block indexes into [2 x [453 x i32]] (stride 1812
; bytes) with %sub59, whose recurrence {1052409205,+,4740582} multiplied by
; 1812 is {36,+,-8} (mod 2^32) -- the same SCEVAddRecExpr as the loop IV %i.
; Before the fix, getGEPExpr attached NSW (strengthened to NUW) to {36,+,-8},
; causing IndVarSimplify to fold %cmp = icmp ugt i32 %i, 7 to true.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define i32 @test_gep_inbounds_flag_pollution(i1 %guard, i32 %max_steps, [2 x [453 x i32]]* %table) {
; CHECK-LABEL: @test_gep_inbounds_flag_pollution(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %steps = phi i32 [ 0, %entry ], [ %steps.next, %latch ]
; CHECK-NEXT:    %i = phi i32 [ 36, %entry ], [ %i.next, %latch ]
; CHECK-NEXT:    %sub = sub nuw nsw i32 36, %i
; CHECK-NEXT:    %cmp = icmp ugt i32 %i, 7
; CHECK-NEXT:    br i1 %cmp, label %body, label %exit
entry:
  br label %loop

loop:
  %steps = phi i32 [ 0, %entry ], [ %steps.next, %latch ]
  %i = phi i32 [ 36, %entry ], [ %i.next, %latch ]
  %sub = sub i32 36, %i
  %cmp = icmp ugt i32 %i, 7
  br i1 %cmp, label %body, label %exit

body:
  %shr = lshr i32 %sub, 3
  %mul = mul i32 %shr, 9481164
  %shl = add i32 %mul, 2104818412
  br i1 %guard, label %guarded, label %latch

guarded:
  %shr51 = lshr exact i32 %shl, 1
  %sub59 = add nsw i32 %shr51, -1
  %gep = getelementptr inbounds [2 x [453 x i32]], [2 x [453 x i32]]* %table, i32 0, i32 %sub59, i32 0
  %v = load i32, i32* %gep, align 4
  br label %latch

latch:
  %steps.next = add i32 %steps, 1
  %i.next = add i32 %i, -8
  %be = icmp ult i32 %steps.next, %max_steps
  br i1 %be, label %loop, label %exit

exit:
  %res = phi i32 [ %i, %loop ], [ %i, %latch ]
  ret i32 %res
}
