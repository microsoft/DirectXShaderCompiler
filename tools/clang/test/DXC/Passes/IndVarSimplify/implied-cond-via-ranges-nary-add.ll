; RUN: %dxopt %s -indvars -S | FileCheck %s

; Regression test for ScalarEvolution::isImpliedCondOperandsViaRanges ignoring
; trailing operands of an n-ary SCEVAddExpr when checking if LHS is FoundLHS + C
; (fixed upstream via splitBinaryAdd in
; https://github.com/llvm/llvm-project/commit/a7d9ec8751b1bb045f83934e9be202a191249db8).
;
; In @nary_add_start_not_implied, %start = (%a * 3 + %b) * 4 produces a
; 3-operand SCEVAddExpr (4 + (4 * %b) + (12 * %a)) for (%off + 4) at loop entry.
; With the dominating guard (4 * %b) <u 201, ScalarEvolution previously ignored
; (12 * %a) and wrongly concluded (%off + 4) <=u 256 on every iteration,
; allowing IndVarSimplify to fold %oob to false.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define i32 @nary_add_start_not_implied(i32 %a, i32 %b, [64 x i32] addrspace(3)* %wg) {
; CHECK-LABEL: @nary_add_start_not_implied(
; CHECK:       loop:
; CHECK-NEXT:    %off = phi i32 [ %start, %guard.true ], [ %off.next, %loop ]
; CHECK-NEXT:    %acc = phi i32 [ 0, %guard.true ], [ %acc.next, %loop ]
; CHECK-NEXT:    %off.plus4 = add i32 %off, 4
; CHECK-NEXT:    %oob = icmp ugt i32 %off.plus4, 256
; CHECK-NEXT:    %idx = lshr i32 %off, 2
; CHECK-NEXT:    %clamped = select i1 %oob, i32 0, i32 %idx
entry:
  %b4 = shl i32 %b, 2
  %guard = icmp ult i32 %b4, 201
  br i1 %guard, label %guard.true, label %exit

guard.true:
  %a3 = mul i32 %a, 3
  %sum = add i32 %a3, %b
  %start = shl i32 %sum, 2
  br label %loop

loop:
  %off = phi i32 [ %start, %guard.true ], [ %off.next, %loop ]
  %acc = phi i32 [ 0, %guard.true ], [ %acc.next, %loop ]
  %off.plus4 = add i32 %off, 4
  %oob = icmp ugt i32 %off.plus4, 256
  %idx = lshr i32 %off, 2
  %clamped = select i1 %oob, i32 0, i32 %idx
  %gep = getelementptr [64 x i32], [64 x i32] addrspace(3)* %wg, i32 0, i32 %clamped
  %val = load i32, i32 addrspace(3)* %gep, align 4
  %acc.next = add i32 %acc, %val
  %off.next = add i32 %off, 4
  %exit.add = add i32 %off, 8
  %exit.cond = icmp ugt i32 %exit.add, 256
  br i1 %exit.cond, label %exit, label %loop

exit:
  %res = phi i32 [ 0, %entry ], [ %acc.next, %loop ]
  ret i32 %res
}

define i32 @binary_add_start_still_implied(i32 %b, [64 x i32] addrspace(3)* %wg) {
; CHECK-LABEL: @binary_add_start_still_implied(
; CHECK:       loop:
; CHECK-NEXT:    %off = phi i32 [ %b4, %guard.true ], [ %off.next, %loop ]
; CHECK-NEXT:    %acc = phi i32 [ 0, %guard.true ], [ %acc.next, %loop ]
; CHECK-NEXT:    %idx = lshr i32 %off, 2
; CHECK-NEXT:    %clamped = select i1 false, i32 0, i32 %idx
entry:
  %b4 = shl i32 %b, 2
  %guard = icmp ult i32 %b4, 201
  br i1 %guard, label %guard.true, label %exit

guard.true:
  br label %loop

loop:
  %off = phi i32 [ %b4, %guard.true ], [ %off.next, %loop ]
  %acc = phi i32 [ 0, %guard.true ], [ %acc.next, %loop ]
  %off.plus4 = add i32 %off, 4
  %oob = icmp ugt i32 %off.plus4, 256
  %idx = lshr i32 %off, 2
  %clamped = select i1 %oob, i32 0, i32 %idx
  %gep = getelementptr [64 x i32], [64 x i32] addrspace(3)* %wg, i32 0, i32 %clamped
  %val = load i32, i32 addrspace(3)* %gep, align 4
  %acc.next = add i32 %acc, %val
  %off.next = add i32 %off, 4
  %exit.add = add i32 %off, 8
  %exit.cond = icmp ugt i32 %exit.add, 256
  br i1 %exit.cond, label %exit, label %loop

exit:
  %res = phi i32 [ 0, %entry ], [ %acc.next, %loop ]
  ret i32 %res
}
