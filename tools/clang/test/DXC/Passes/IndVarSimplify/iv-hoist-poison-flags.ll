; RUN: %dxopt %s -indvars -S | FileCheck %s

; When SCEVExpander::replaceCongruentIVs replaces an isomorphic induction
; variable increment by hoisting the canonical increment above it
; (hoistIVInc), the canonical increment gets new uses. Its poison-generating
; flags may only have held at its original position, e.g. below a loop exit
; guard, so they are dropped.
;
; Based on:
; https://github.com/llvm/llvm-project/commit/86d5586d78d813a6921d786f7ddb1a41c6fb56e0 (PR57187)

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; Hoisting %inc_i (add nsw) from the latch above the loop exit guard to replace
; %inc_u (add without nsw) must drop nsw, because %inc_u executes on the first
; iteration before the exit guard when %start_nonneg can be INT32_MAX.
define i32 @hoist_iv_inc_across_guard_drops_nsw(i32 %start) {
; CHECK-LABEL: @hoist_iv_inc_across_guard_drops_nsw(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    %start_nonneg = and i32 %start, 2147483647
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %i = phi i32 [ %start_nonneg, %entry ], [ %inc_i, %latch ]
; CHECK-NEXT:    %inc_i = add i32 %i, 1
; CHECK-NEXT:    %cond = icmp sgt i32 %i, 9
; CHECK-NEXT:    br i1 %cond, label %exit, label %latch
; CHECK:       latch:
; CHECK-NEXT:    br i1 true, label %loop, label %exit
; CHECK:       exit:
; CHECK-NEXT:    %res = phi i32 [ %inc_i, %loop ], [ %inc_i, %latch ]
; CHECK-NEXT:    ret i32 %res
entry:
  %start_nonneg = and i32 %start, 2147483647
  br label %loop

loop:
  %i = phi i32 [ %start_nonneg, %entry ], [ %inc_i, %latch ]
  %u = phi i32 [ %start_nonneg, %entry ], [ %inc_u, %latch ]
  %g = phi i32 [ -1, %entry ], [ %inc_g, %latch ]
  %inc_u = add i32 %u, 1
  %cond = icmp sgt i32 %i, 9
  br i1 %cond, label %exit, label %latch

latch:
  %inc_i = add nsw i32 %i, 1
  %inc_g = add nsw i32 %g, 1
  %backedge_cond = icmp slt i32 %inc_g, 1000
  br i1 %backedge_cond, label %loop, label %exit

exit:
  %res = phi i32 [ %inc_u, %loop ], [ %inc_u, %latch ]
  ret i32 %res
}

; Like above, but the overflow happens on the second iteration: with %c true,
; %inc_u computes INT32_MAX + 1 in the header before the exit guard is taken.
; ScalarEvolution's add recurrences for %i and %u carry %inc_i's nsw, so
; re-inferring flags from them at the new position would restore it.
define i32 @hoist_iv_inc_across_guard_one_iteration(i1 %c) {
; CHECK-LABEL: @hoist_iv_inc_across_guard_one_iteration(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %i = phi i32 [ 2147483646, %entry ], [ %inc_i, %latch ]
; CHECK-NEXT:    %inc_i = add i32 %i, 1
; CHECK-NEXT:    %cond = icmp sgt i32 %i, 2147483646
; CHECK-NEXT:    br i1 %cond, label %exit, label %latch
; CHECK:       latch:
; CHECK-NEXT:    br i1 %c, label %loop, label %exit
; CHECK:       exit:
; CHECK-NEXT:    %res = phi i32 [ %inc_i, %loop ], [ %inc_i, %latch ]
; CHECK-NEXT:    ret i32 %res
entry:
  br label %loop

loop:
  %i = phi i32 [ 2147483646, %entry ], [ %inc_i, %latch ]
  %u = phi i32 [ 2147483646, %entry ], [ %inc_u, %latch ]
  %inc_u = add i32 %u, 1
  %cond = icmp sgt i32 %i, 2147483646
  br i1 %cond, label %exit, label %latch

latch:
  %inc_i = add nsw i32 %i, 1
  br i1 %c, label %loop, label %exit

exit:
  %res = phi i32 [ %inc_u, %loop ], [ %inc_u, %latch ]
  ret i32 %res
}

; Hoisting a pointer increment above the loop exit guard must drop inbounds.
define i32* @hoist_gep_iv_inc_drops_inbounds(i32* %base, i1 %c0, i1 %c1) {
; CHECK-LABEL: @hoist_gep_iv_inc_drops_inbounds(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %p = phi i32* [ %base, %entry ], [ %p.next, %latch ]
; CHECK-NEXT:    %p.next = getelementptr i32, i32* %p, i32 1
; CHECK-NEXT:    br i1 %c0, label %exit, label %latch
; CHECK:       exit:
; CHECK-NEXT:    %res = phi i32* [ %p.next, %loop ], [ %p.next, %latch ]
; CHECK-NEXT:    ret i32* %res
entry:
  br label %loop

loop:
  %p = phi i32* [ %base, %entry ], [ %p.next, %latch ]
  %q = phi i32* [ %base, %entry ], [ %q.next, %latch ]
  %q.next = getelementptr i32, i32* %q, i32 1
  br i1 %c0, label %exit, label %latch

latch:
  %p.next = getelementptr inbounds i32, i32* %p, i32 1
  br i1 %c1, label %loop, label %exit

exit:
  %res = phi i32* [ %q.next, %loop ], [ %q.next, %latch ]
  ret i32* %res
}
