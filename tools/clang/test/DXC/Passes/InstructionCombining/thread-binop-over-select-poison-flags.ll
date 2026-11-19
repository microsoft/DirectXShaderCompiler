; RUN: opt < %s -instsimplify -S | FileCheck %s --check-prefixes=CHECK,INSTSIMPLIFY
; RUN: %dxopt %s -instcombine -S | FileCheck %s --check-prefixes=CHECK,INSTCOMBINE

; Regression test for ThreadBinOpOverSelect not checking poison-generating flags
; when reusing an instruction from one select arm for both arms (backport of
; https://github.com/llvm/llvm-project/commit/3197f9d8b0efc3efdc531421bd11c16305d9b1ff).

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

declare void @use(i32)

; With %cond = false and %x = 0x1f000, the result is 15. Reusing %shl1 for
; %shl2 leaks nuw onto the false arm and lets InstCombine drop the '& 15' mask
; (returning 0x1f000 >> 12 = 31).
define i32 @shl_nuw_not_reused(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @shl_nuw_not_reused(
; INSTSIMPLIFY-NEXT: %shl1 = shl nuw i32 %x, 16
; INSTSIMPLIFY-NEXT: %ashr = ashr exact i32 %shl1, 16
; INSTSIMPLIFY-NEXT: %sel = select i1 %cond, i32 %ashr, i32 %x
; INSTSIMPLIFY-NEXT: %shl2 = shl i32 %sel, 16
; INSTSIMPLIFY-NEXT: %hi = lshr i32 %shl2, 28
; INSTSIMPLIFY-NEXT: ret i32 %hi
; INSTCOMBINE-NEXT:  %1 = lshr i32 %x, 12
; INSTCOMBINE-NEXT:  %hi = and i32 %1, 15
; INSTCOMBINE-NEXT:  ret i32 %hi
  %shl1 = shl nuw i32 %x, 16
  %ashr = ashr exact i32 %shl1, 16
  %sel = select i1 %cond, i32 %ashr, i32 %x
  %shl2 = shl i32 %sel, 16
  %hi = lshr i32 %shl2, 28
  ret i32 %hi
}

; Same pattern for nsw on shl: InstSimplify must not fold %shl2 to %shl1.
define i32 @shl_nsw_not_reused(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @shl_nsw_not_reused(
; INSTSIMPLIFY-NEXT: %shl1 = shl nsw i32 %x, 16
; INSTSIMPLIFY-NEXT: %lshr = lshr exact i32 %shl1, 16
; INSTSIMPLIFY-NEXT: %sel = select i1 %cond, i32 %lshr, i32 %x
; INSTSIMPLIFY-NEXT: %shl2 = shl i32 %sel, 16
; INSTSIMPLIFY-NEXT: %hi = ashr i32 %shl2, 28
; INSTSIMPLIFY-NEXT: ret i32 %hi
; INSTCOMBINE-NEXT:  %shl2 = shl i32 %x, 16
; INSTCOMBINE-NEXT:  %hi = ashr i32 %shl2, 28
; INSTCOMBINE-NEXT:  ret i32 %hi
  %shl1 = shl nsw i32 %x, 16
  %lshr = lshr exact i32 %shl1, 16
  %sel = select i1 %cond, i32 %lshr, i32 %x
  %shl2 = shl i32 %sel, 16
  %hi = ashr i32 %shl2, 28
  ret i32 %hi
}

; Direct operand match with nuw on mul: with %cond = false, %x = 2, and
; %y = 0x80000000, the result is 0, whereas reusing %mul1 folds %q to %y.
define i32 @mul_nuw_direct_not_reused(i1 %cond, i32 %x, i32 %y) {
; CHECK-LABEL: define i32 @mul_nuw_direct_not_reused(
; CHECK-NEXT:    %mul1 = mul nuw i32 %x, %y
; CHECK-NEXT:    %div = sdiv exact i32 %mul1, %y
; CHECK-NEXT:    %sel = select i1 %cond, i32 %div, i32 %x
; CHECK-NEXT:    %mul2 = mul i32 %sel, %y
; CHECK-NEXT:    %q = udiv i32 %mul2, %x
; CHECK-NEXT:    ret i32 %q
  %mul1 = mul nuw i32 %x, %y
  %div = sdiv exact i32 %mul1, %y
  %sel = select i1 %cond, i32 %div, i32 %x
  %mul2 = mul i32 %sel, %y
  %q = udiv i32 %mul2, %x
  ret i32 %q
}

; Commuted operand match with nsw on mul: with %cond = false, %x = 2, and
; %y = 0x40000000, the result is -0x40000000, whereas reusing %mul1 folds %q to
; %y.
define i32 @mul_nsw_commuted_not_reused(i1 %cond, i32 %x, i32 %y) {
; CHECK-LABEL: define i32 @mul_nsw_commuted_not_reused(
; CHECK-NEXT:    %mul1 = mul nsw i32 %y, %x
; CHECK-NEXT:    %div = udiv exact i32 %mul1, %y
; CHECK-NEXT:    %sel = select i1 %cond, i32 %div, i32 %x
; CHECK-NEXT:    %mul2 = mul i32 %sel, %y
; CHECK-NEXT:    %q = sdiv i32 %mul2, %x
; CHECK-NEXT:    ret i32 %q
  %mul1 = mul nsw i32 %y, %x
  %div = udiv exact i32 %mul1, %y
  %sel = select i1 %cond, i32 %div, i32 %x
  %mul2 = mul i32 %sel, %y
  %q = sdiv i32 %mul2, %x
  ret i32 %q
}

; Direct operand match with exact on ashr: on the true arm, %ashr1 has all sign
; bits so (%ashr1 >>s 31) simplifies to %ashr1. Reusing %ashr1 for %ashr2 leaks
; exact onto the false arm and folds %back = (%ashr2 << 31) to %x (returning 1
; instead of 0 for %cond = false, %x = 1).
define i32 @ashr_exact_not_reused(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @ashr_exact_not_reused(
; INSTSIMPLIFY-NEXT: %ashr1 = ashr exact i32 %x, 31
; INSTSIMPLIFY-NEXT: %sel = select i1 %cond, i32 %ashr1, i32 %x
; INSTSIMPLIFY-NEXT: %ashr2 = ashr i32 %sel, 31
; INSTSIMPLIFY-NEXT: %back = shl i32 %ashr2, 31
; INSTSIMPLIFY-NEXT: ret i32 %back
; INSTCOMBINE-NEXT:  %ashr2.1 = and i32 %x, -2147483648
; INSTCOMBINE-NEXT:  ret i32 %ashr2.1
  %ashr1 = ashr exact i32 %x, 31
  %sel = select i1 %cond, i32 %ashr1, i32 %x
  %ashr2 = ashr i32 %sel, 31
  %back = shl i32 %ashr2, 31
  ret i32 %back
}

; Without poison-generating flags, ThreadBinOpOverSelect still reuses the
; existing instruction for direct, commuted, and exact-capable opcodes.
define i32 @shl_without_flags_reused(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @shl_without_flags_reused(
; CHECK-NEXT:    %shl1 = shl i32 %x, 16
; CHECK-NEXT:    call void @use(i32 %shl1)
; CHECK-NEXT:    ret i32 %shl1
  %shl1 = shl i32 %x, 16
  call void @use(i32 %shl1)
  %ashr = ashr exact i32 %shl1, 16
  %sel = select i1 %cond, i32 %ashr, i32 %x
  %shl2 = shl i32 %sel, 16
  ret i32 %shl2
}

define i32 @mul_commuted_without_flags_reused(i1 %cond, i32 %x, i32 %y) {
; CHECK-LABEL: define i32 @mul_commuted_without_flags_reused(
; CHECK-NEXT:    %mul1 = mul i32 %y, %x
; CHECK-NEXT:    call void @use(i32 %mul1)
; CHECK-NEXT:    ret i32 %mul1
  %mul1 = mul i32 %y, %x
  call void @use(i32 %mul1)
  %div = udiv exact i32 %mul1, %y
  %sel = select i1 %cond, i32 %div, i32 %x
  %mul2 = mul i32 %sel, %y
  ret i32 %mul2
}

define i32 @ashr_without_flags_reused(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @ashr_without_flags_reused(
; CHECK-NEXT:    %ashr1 = ashr i32 %x, 31
; CHECK-NEXT:    call void @use(i32 %ashr1)
; CHECK-NEXT:    ret i32 %ashr1
  %ashr1 = ashr i32 %x, 31
  call void @use(i32 %ashr1)
  %sel = select i1 %cond, i32 %ashr1, i32 %x
  %ashr2 = ashr i32 %sel, 31
  ret i32 %ashr2
}
