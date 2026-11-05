; RUN: %dxopt %s -instcombine -S | FileCheck %s

; Regression test for the backport of
; https://github.com/llvm/llvm-project/commit/56df0ec61033a5bb49b3620e70cc6a61e26b3e7a
; (rL264996), copied unchanged. matchSelectPattern matched (X pred C) ? ~X : ~C
; as smin(~X, ~C) regardless of the predicate, but that only holds for sgt.
; Before the fix, InstCombine folded @PR27137 to 'ret i32 -1', which is wrong
; for negative %a.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define i32 @PR27137(i32 %a) {
; CHECK-LABEL: @PR27137(
; CHECK-NEXT:  %not_a = xor i32 %a, -1
; CHECK-NEXT:  %c0 = icmp slt i32 %a, 0
; CHECK-NEXT:  %s0 = select i1 %c0, i32 %not_a, i32 -1
; CHECK-NEXT:  %c1 = icmp sgt i32 %s0, -1
; CHECK-NEXT:  %s1 = select i1 %c1, i32 %s0, i32 -1
; CHECK-NEXT:  ret i32 %s1

  %not_a = xor i32 %a, -1
  %c0 = icmp slt i32 %a, 0
  %s0 = select i1 %c0, i32 %not_a, i32 -1
  %c1 = icmp sgt i32 %s0, -1
  %s1 = select i1 %c1, i32 %s0, i32 -1
  ret i32 %s1
}
