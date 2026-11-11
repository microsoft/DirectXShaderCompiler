; RUN: %dxopt %s -analyze -scalar-evolution | FileCheck %s

; The no-wrap flags of an IV increment may only be applied to the
; post-increment add recurrence when overflowing the increment would be
; undefined behavior: the loop's only exiting block is the latch, the latch
; condition depends on the increment, and nothing in the loop may throw.
; Otherwise other instructions computing the same value, on iterations where
; the increment does not execute, would inherit its flags.
; Partial backport of PR27315:
; https://github.com/llvm/llvm-project/commit/7e4a64167d4d2e7b0b680fae1706182223047af1

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

declare void @may_throw()

; CHECK-LABEL: Classifying expressions for: @latch_exit_uses_inc
; CHECK:       %inc = add nsw i32 %i, 1
; CHECK-NEXT:  -->  {(1 + %start),+,1}<nsw><%loop>
define void @latch_exit_uses_inc(i32 %start, i32 %n) {
entry:
  br label %loop

loop:
  %i = phi i32 [ %start, %entry ], [ %inc, %loop ]
  %inc = add nsw i32 %i, 1
  %cmp = icmp slt i32 %inc, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

; CHECK-LABEL: Classifying expressions for: @header_exit_guards_inc
; CHECK:       %inc = add nsw i32 %i, 1
; CHECK-NEXT:  -->  {(1 + %start),+,1}<nw><%loop>
define void @header_exit_guards_inc(i32 %start, i1 %c) {
entry:
  br label %loop

loop:
  %i = phi i32 [ %start, %entry ], [ %inc, %latch ]
  %cond = icmp sgt i32 %i, 9
  br i1 %cond, label %exit, label %latch

latch:
  %inc = add nsw i32 %i, 1
  br i1 %c, label %loop, label %exit

exit:
  ret void
}

; CHECK-LABEL: Classifying expressions for: @latch_exit_ignores_inc
; CHECK:       %inc = add nsw i32 %i, 1
; CHECK-NEXT:  -->  {(1 + %start),+,1}<nw><%loop>
define void @latch_exit_ignores_inc(i32 %start, i1 %c) {
entry:
  br label %loop

loop:
  %i = phi i32 [ %start, %entry ], [ %inc, %loop ]
  %inc = add nsw i32 %i, 1
  br i1 %c, label %loop, label %exit

exit:
  ret void
}

; CHECK-LABEL: Classifying expressions for: @loop_may_throw
; CHECK:       %inc = add nsw i32 %i, 1
; CHECK-NEXT:  -->  {(1 + %start),+,1}<nw><%loop>
define void @loop_may_throw(i32 %start, i32 %n) {
entry:
  br label %loop

loop:
  %i = phi i32 [ %start, %entry ], [ %inc, %loop ]
  call void @may_throw()
  %inc = add nsw i32 %i, 1
  %cmp = icmp slt i32 %inc, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}
