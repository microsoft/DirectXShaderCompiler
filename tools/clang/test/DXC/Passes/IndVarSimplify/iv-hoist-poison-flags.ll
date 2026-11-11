; RUN: %dxopt %s -indvars -S | FileCheck %s

; When SCEVExpander::replaceCongruentIVs replaces an isomorphic induction
; variable increment with the canonical increment, either by hoisting the
; canonical increment above it (hoistIVInc) or by reusing a canonical increment
; that already dominates it, the canonical increment gets new uses. Its
; poison-generating flags may only have held at its original position, e.g.
; below a loop exit guard, so they are dropped. Only nuw/nsw flags that both
; increments already carry are kept.
;
; Based on:
; https://github.com/llvm/llvm-project/commit/86d5586d78d813a6921d786f7ddb1a41c6fb56e0 (PR57187)
; https://github.com/llvm/llvm-project/commit/90c184662925b8f17bc9cf05cd60a9e98201fff6 (PR59777)
; https://github.com/llvm/llvm-project/commit/1f9de23e94af3ff50f67a3b16be3769919c2e146
; https://github.com/llvm/llvm-project/commit/da437330beca0411912ca46f2d5ef1e46403acbd (#79512)

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
; CHECK-NEXT:    %inc_i = add nuw i32 %i, 1
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
; The add that replaces it must not be nsw, and nsw must not be re-derived from
; ScalarEvolution, whose add recurrences for %i and %u carry %inc_i's flags.
define i32 @hoist_iv_inc_across_guard_one_iteration(i1 %c) {
; CHECK-LABEL: @hoist_iv_inc_across_guard_one_iteration(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %i = phi i32 [ 2147483646, %entry ], [ %inc_i, %latch ]
; CHECK-NEXT:    %inc_i = add nuw i32 %i, 1
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

; Opposite phi order: %inc_u already dominates %inc_i and replaces it. %inc_u
; must not become nsw because of %inc_i, which only executes below the guard.
define i32 @reuse_dominating_iv_inc_across_guard(i1 %c) {
; CHECK-LABEL: @reuse_dominating_iv_inc_across_guard(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %u = phi i32 [ 2147483646, %entry ], [ %inc_u, %latch ]
; CHECK-NEXT:    %inc_u = add nuw i32 %u, 1
; CHECK-NEXT:    %cond = icmp sgt i32 %u, 2147483646
; CHECK-NEXT:    br i1 %cond, label %exit, label %latch
; CHECK:       latch:
; CHECK-NEXT:    br i1 %c, label %loop, label %exit
; CHECK:       exit:
; CHECK-NEXT:    %res = phi i32 [ %inc_u, %loop ], [ %inc_u, %latch ]
; CHECK-NEXT:    ret i32 %res
entry:
  br label %loop

loop:
  %u = phi i32 [ 2147483646, %entry ], [ %inc_u, %latch ]
  %i = phi i32 [ 2147483646, %entry ], [ %inc_i, %latch ]
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

; Both increments are nsw, so the hoisted increment keeps nsw.
define void @hoist_iv_inc_keeps_common_nsw(i32 %start, i32 %n, i32* %p) {
; CHECK-LABEL: @hoist_iv_inc_keeps_common_nsw(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %i = phi i32 [ %start, %entry ], [ %inc_i, %loop ]
; CHECK-NEXT:    %inc_i = add nsw i32 %i, 1
; CHECK-NEXT:    store i32 %inc_i, i32* %p
; CHECK-NEXT:    %cmp = icmp slt i32 %inc_i, %n
; CHECK-NEXT:    br i1 %cmp, label %loop, label %exit
entry:
  br label %loop

loop:
  %i = phi i32 [ %start, %entry ], [ %inc_i, %loop ]
  %u = phi i32 [ %start, %entry ], [ %inc_u, %loop ]
  %inc_u = add nsw i32 %u, 1
  store i32 %inc_u, i32* %p
  %inc_i = add nsw i32 %i, 1
  %cmp = icmp slt i32 %inc_i, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
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

; Adapted from llvm/test/Transforms/IndVarSimplify/iv-poison.ll (PR59777),
; widened from i2 (INT2_MAX = 1) to i32 (INT32_MAX = 2147483647).
; Even when %i already dominates %i2, replacing %i2 with %i introduces a new
; use of %i, so nsw, which only %i carries, must be dropped. Both increments
; are nuw by then, so nuw is kept.
define i32 @iv_hoist_nsw_poison(i32 %arg) {
; CHECK-LABEL: @iv_hoist_nsw_poison(
; CHECK-NEXT:  bb:
; CHECK-NEXT:    br label %bb1
; CHECK:       bb1:
; CHECK-NEXT:    %.07 = phi i32 [ 2147483647, %bb ], [ %i, %bb1 ]
; CHECK-NEXT:    %i = add nuw i32 %.07, 1
; CHECK-NEXT:    %.not.not = icmp ult i32 %.07, %arg
; CHECK-NEXT:    br i1 %.not.not, label %common.ret, label %bb1
; CHECK:       common.ret:
; CHECK-NEXT:    %i2.lcssa = phi i32 [ %i, %bb1 ]
; CHECK-NEXT:    ret i32 %i2.lcssa
bb:
  br label %bb1

bb1:
  %.07 = phi i32 [ 2147483647, %bb ], [ %i, %bb1 ]
  %.0 = phi i32 [ 2147483647, %bb ], [ %i2, %bb1 ]
  %i = add nsw i32 %.07, 1
  %i2 = add i32 %.0, 1
  %.not.not = icmp ult i32 %.07, %arg
  br i1 %.not.not, label %common.ret, label %bb1

common.ret:
  ret i32 %i2
}

; Adapted from llvm/test/Transforms/IndVarSimplify/iv-poison.ll, widened from
; i4 to i32. The flagless %iv.0.next replaces %iv.1.next and must not become
; nsw.
define i32 @iv_hoist_nsw_poison2(i32 %end, i32 %start) {
; CHECK-LABEL: @iv_hoist_nsw_poison2(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %iv.0 = phi i32 [ %start, %entry ], [ %iv.0.next, %loop ]
; CHECK-NEXT:    %iv.0.next = add i32 %iv.0, 1
; CHECK-NEXT:    %.not.not = icmp ult i32 %iv.0, %end
; CHECK-NEXT:    br i1 %.not.not, label %exit, label %loop
; CHECK:       exit:
; CHECK-NEXT:    %iv.1.next.lcssa = phi i32 [ %iv.0.next, %loop ]
; CHECK-NEXT:    ret i32 %iv.1.next.lcssa
entry:
  br label %loop

loop:
  %iv.0 = phi i32 [ %start, %entry ], [ %iv.0.next, %loop ]
  %iv.1 = phi i32 [ %start, %entry ], [ %iv.1.next, %loop ]
  %iv.0.next = add i32 %iv.0, 1
  %iv.1.next = add nsw i32 %iv.1, 1
  %.not.not = icmp ult i32 %iv.0, %end
  br i1 %.not.not, label %exit, label %loop

exit:
  ret i32 %iv.1.next
}
