; RUN: %opt %s -instcombine -S | FileCheck %s

; Backport of https://github.com/llvm/llvm-project/commit/56df0ec61033a5bb49b3620e70cc6a61e26b3e7a (rL264996).
define i32 @PR27137(i32 %a) {
; CHECK-LABEL: @PR27137(
; CHECK-NEXT:  [[NOT_A:%[a-zA-Z0-9_]+]] = xor i32 %a, -1
; CHECK-NEXT:  [[C0:%[a-zA-Z0-9_]+]] = icmp slt i32 %a, 0
; CHECK-NEXT:  [[S0:%[a-zA-Z0-9_]+]] = select i1 [[C0]], i32 [[NOT_A]], i32 -1
; CHECK-NEXT:  ret i32 [[S0]]
  %not_a = xor i32 %a, -1
  %c0 = icmp slt i32 %a, 0
  %s0 = select i1 %c0, i32 %not_a, i32 -1
  %c1 = icmp sgt i32 %s0, -1
  %s1 = select i1 %c1, i32 %s0, i32 -1
  ret i32 %s1
}
