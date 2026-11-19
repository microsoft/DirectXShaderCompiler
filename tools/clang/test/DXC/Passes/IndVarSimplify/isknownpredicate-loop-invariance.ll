; RUN: %dxopt %s -indvars -S | FileCheck %s
; RUN: opt < %s -scalar-evolution -analyze | FileCheck %s --check-prefix=SCEV

; Regression tests for ScalarEvolution::isKnownPredicate induction reasoning
; (partial backport of D42417 and D43997):
;   - Non-AddRec operands must be invariant in (and properly dominate the header
;     of) the AddRec's loop (i.e., available at loop entry).

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

declare i32 @get_val(i32)
declare void @use_i1(i1)

; Bug 586171733: %x is recomputed every iteration of %outer (%raw & 0x7fffffff).
; At outer entry, %x <=u 0x80000000 holds, and the outer latch checks
; %i1.next >=u %x, so unpatched isKnownPredicate(ICMP_ULE, %x, %i1) proved true
; without checking that %x is invariant in %outer. That folded umax(%x, %i1) to
; %i1 in SCEV for %sel, giving %j the wrong SCEV {%i1 + %d, +, 1}<%inner>, which
; together with %guard (%i1 + %d <u 5) and inner latch (%j + 1 <u 5) caused
; SimplifyIndVar::eliminateIVRemainder to delete `urem i32 %j, 5`.
define void @urem_not_eliminated_with_loop_variant_select_bound([8 x i32] addrspace(3)* %arr) {
; CHECK-LABEL: @urem_not_eliminated_with_loop_variant_select_bound(
; CHECK:       inner:
; CHECK-NEXT:    %j = phi i32 [ %sel, %inner.preheader ], [ %j.next, %inner ]
; CHECK-NEXT:    %rem = urem i32 %j, 5
; CHECK-NEXT:    %gep = getelementptr inbounds [8 x i32], [8 x i32] addrspace(3)* %arr, i32 0, i32 %rem
entry:
  br label %outer

outer:
  %iter = phi i32 [ 0, %entry ], [ %iter.next, %outer.latch ]
  %i1 = phi i32 [ -2147483648, %entry ], [ %i1.next, %outer.latch ]
  %raw = call i32 @get_val(i32 %iter)
  %x = and i32 %raw, 2147483647
  %iter.next = add i32 %iter, 1
  %d = call i32 @get_val(i32 %iter.next)
  %cond = add i32 %i1, %d
  %guard = icmp ult i32 %cond, 5
  br i1 %guard, label %inner.preheader, label %outer.latch

inner.preheader:
  %cmp = icmp ugt i32 %i1, %x
  %add_x = add i32 %x, %d
  %sel = select i1 %cmp, i32 %cond, i32 %add_x
  br label %inner

inner:
  %j = phi i32 [ %sel, %inner.preheader ], [ %j.next, %inner ]
  %rem = urem i32 %j, 5
  %gep = getelementptr inbounds [8 x i32], [8 x i32] addrspace(3)* %arr, i32 0, i32 %rem
  store i32 1094795585, i32 addrspace(3)* %gep, align 4
  %j.next = add i32 %j, 1
  %inner.cond = icmp ult i32 %j.next, 5
  br i1 %inner.cond, label %inner, label %outer.latch

outer.latch:
  %i1.next = xor i32 %i1, -2147483648
  %exit = icmp ult i32 %i1.next, %x
  br i1 %exit, label %ret, label %outer

ret:
  ret void
}

; Single-loop induction variable (LAR) compared against loop-variant %x (RHS)
; matching the latch condition must not fold to true.
define i32 @icmp_not_folded_with_loop_variant_rhs() {
; CHECK-LABEL: @icmp_not_folded_with_loop_variant_rhs(
; CHECK:       loop:
; CHECK:         %cmp = icmp uge i32 %i, %x
; CHECK-NEXT:    %hit = zext i1 %cmp to i32
entry:
  br label %loop

loop:
  %iter = phi i32 [ 0, %entry ], [ %iter.next, %loop ]
  %i = phi i32 [ -2147483648, %entry ], [ %i.next, %loop ]
  %hits = phi i32 [ 0, %entry ], [ %hits.next, %loop ]
  %raw = call i32 @get_val(i32 %iter)
  %x = and i32 %raw, 2147483647
  %cmp = icmp uge i32 %i, %x
  %hit = zext i1 %cmp to i32
  %hits.next = add i32 %hits, %hit
  %i.next = xor i32 %i, -2147483648
  %iter.next = add i32 %iter, 1
  %exit = icmp ult i32 %i.next, %x
  br i1 %exit, label %ret, label %loop

ret:
  ret i32 %hits.next
}

; Symmetric case: loop-variant %x on LHS, AddRec %i on RHS (RAR).
define i32 @icmp_not_folded_with_loop_variant_lhs() {
; CHECK-LABEL: @icmp_not_folded_with_loop_variant_lhs(
; CHECK:       loop:
; CHECK:         %cmp = icmp ule i32 %x, %i
; CHECK-NEXT:    %hit = zext i1 %cmp to i32
entry:
  br label %loop

loop:
  %iter = phi i32 [ 0, %entry ], [ %iter.next, %loop ]
  %i = phi i32 [ -2147483648, %entry ], [ %i.next, %loop ]
  %hits = phi i32 [ 0, %entry ], [ %hits.next, %loop ]
  %raw = call i32 @get_val(i32 %iter)
  %x = and i32 %raw, 2147483647
  %cmp = icmp ule i32 %x, %i
  %hit = zext i1 %cmp to i32
  %hits.next = add i32 %hits, %hit
  %i.next = xor i32 %i, -2147483648
  %iter.next = add i32 %iter, 1
  %exit = icmp ult i32 %i.next, %x
  br i1 %exit, label %ret, label %loop

ret:
  ret i32 %hits.next
}

; Positive test: a bound %n that IS loop-invariant and dominates the loop header
; should still fold %cmp to true via induction.
define void @loop_invariant_bound_folds(i32 %start, i32 %n) {
; CHECK-LABEL: @loop_invariant_bound_folds(
; CHECK:       loop:
; CHECK-NOT:     icmp ult i32 %i, %n
; CHECK:         call void @use_i1(i1 true)
entry:
  %entry.guard = icmp ult i32 %start, %n
  br i1 %entry.guard, label %loop, label %exit

loop:
  %i = phi i32 [ %start, %entry ], [ %i.next, %loop ]
  %cmp = icmp ult i32 %i, %n
  call void @use_i1(i1 %cmp)
  %i.next = add i32 %i, 1
  %be = icmp ult i32 %i.next, %n
  br i1 %be, label %loop, label %exit

exit:
  ret void
}

; Dominance check in IsAvailableAtLoopEntry: %b is defined in %middle after
; %first_loop, so !first_loop->contains(%b) makes isLoopInvariant(%b, %first_loop)
; true, while properlyDominates(%b, %first_loop) is false. Because %b is in
; [96, 128), entry (0 <u %b) and the %first_loop latch (%i.next <u 50 <= %b)
; would both succeed under an invariance-only check and fold umax(%i, %b) to %b.
; With properlyDominates, SCEV must keep %s as an unfolded umax.
define void @non_dominating_value_after_loop(i32 %step, [256 x i32] addrspace(3)* %arr) {
; CHECK-LABEL: @non_dominating_value_after_loop(
; CHECK:       middle:
; CHECK:         %s = select i1 %cmp, i32 %{{.*}}, i32 %b
; CHECK:       second_loop:
; CHECK:         %rem = urem i32 %j, 150
; SCEV-LABEL: Classifying expressions for: @non_dominating_value_after_loop
; SCEV:       %s = select i1 %cmp, i32 %i, i32 %b
; SCEV-NEXT:  -->  ({0,+,%step}<%first_loop> umax %b)
entry:
  br label %first_loop

first_loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %first_loop ]
  %i.next = add i32 %i, %step
  %cond1 = icmp ult i32 %i.next, 50
  br i1 %cond1, label %first_loop, label %middle

middle:
  %raw = call i32 @get_val(i32 0)
  %masked = and i32 %raw, 31
  %b = or i32 %masked, 96
  %cmp = icmp ugt i32 %i, %b
  %s = select i1 %cmp, i32 %i, i32 %b
  br label %second_loop

second_loop:
  %j = phi i32 [ %s, %middle ], [ %j.next, %second_loop ]
  %rem = urem i32 %j, 150
  %gep = getelementptr inbounds [256 x i32], [256 x i32] addrspace(3)* %arr, i32 0, i32 %rem
  store i32 1, i32 addrspace(3)* %gep, align 4
  %j.next = add i32 %j, 1
  %cond2 = icmp ult i32 %j.next, 150
  br i1 %cond2, label %second_loop, label %exit

exit:
  ret void
}
