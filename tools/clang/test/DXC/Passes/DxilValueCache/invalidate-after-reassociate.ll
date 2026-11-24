; RUN: opt < %s -analyze -dxil-elim-vector -dxil-value-cache -reassociate -dxil-value-cache | FileCheck %s

; dxil-elim-vector resolves %ext through %X and leaves %X cached as 1.
; reassociate then rewrites %X in place from (%p1 + 1) to (%i + 1), so the
; cache printed after it must be a new, empty one rather than the stale entry.

; CHECK: Printing analysis 'Dxil Value Cache' for function 'f':
; CHECK: %X = add i32 %p1, 1
; CHECK-NEXT: -->  i32 1
; CHECK: Printing analysis 'Dxil Value Cache' for function 'f':
; CHECK-NOT: %X

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
  %vec = insertelement <2 x i32> undef, i32 %X, i32 0
  %ext = extractelement <2 x i32> %vec, i32 0
  call void @sink(i32 %root)
  ret void
}
