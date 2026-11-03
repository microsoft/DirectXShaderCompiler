; RUN: opt < %s -analyze -dxil-value-cache -reassociate -dxil-value-cache | FileCheck %s

; Check that the cache built before reassociate is not reused afterwards.
; reassociate rewrites %X in place from (%p1 + 1) to (%i + 1), so the second
; print must not report the constant computed for the old %X.

; CHECK: Constant values for: @f
; CHECK: %X = add i32 %p1, 1
; CHECK-NEXT: -->  i32 1
; CHECK: Constant values for: @f
; CHECK-NOT: %X = add

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

declare i1 @dx.break()
declare void @sink(i32)

define void @f(i32 %a, i32 %i) {
entry:
  %brk = call i1 @dx.break()
  br i1 %brk, label %branch_merge, label %branch_else

branch_else:
  br label %branch_merge

branch_merge:
  %p1 = phi i32 [ 0, %entry ], [ %a, %branch_else ]
  %X = add i32 %p1, 1
  %root = add i32 %X, %i
  call void @sink(i32 %root)
  ret void
}
