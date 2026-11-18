; RUN: opt -hlsl-dxilload -hlsl-validate-wave-sensitivity -S %s -o /dev/null 2>&1 | FileCheck %s

; Uses opt rather than dxopt because dxopt does not print the pass's warnings.

; The outer latch is only reached through the inner loop, so the outer loop
; header phis can only be resolved after the inner loop phis. Check that the
; analysis still resolves them, and warns only in @nested_sensitive.

; CHECK-NOT: Gradient
; CHECK: Function: nested_sensitive: warning: Gradient operations are not affected by wave-sensitive data or control flow.
; CHECK-NOT: Gradient

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; The gradient operand is not wave sensitive. The pass only runs because
; @nested_sensitive contains a wave op.
define void @nested(float %p, i32 %n) {
entry:
  br label %outer

outer:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  br label %inner

inner:
  %j = phi i32 [ 0, %outer ], [ %j.next, %inner ]
  %f = uitofp i32 %j to float
  %d = call float @dx.op.unary.f32(i32 83, float %f)
  %j.next = add i32 %j, 1
  %j.done = icmp eq i32 %j.next, %n
  br i1 %j.done, label %outer.latch, label %inner

outer.latch:
  %i.next = add i32 %i, 1
  %i.done = icmp eq i32 %i.next, %n
  br i1 %i.done, label %exit, label %outer

exit:
  ret void
}

; The wave op in the outer latch feeds the gradient operand on the next outer
; iteration, so the gradient operand is wave sensitive.
define void @nested_sensitive(float %p, i32 %n) {
entry:
  br label %outer

outer:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  %q = phi float [ %p, %entry ], [ %w, %outer.latch ]
  br label %inner

inner:
  %j = phi i32 [ 0, %outer ], [ %j.next, %inner ]
  %d = call float @dx.op.unary.f32(i32 83, float %q)
  %j.next = add i32 %j, 1
  %j.done = icmp eq i32 %j.next, %n
  br i1 %j.done, label %outer.latch, label %inner

outer.latch:
  %w = call float @dx.op.waveActiveOp.f32(i32 119, float %q, i8 0, i8 0)
  %i.next = add i32 %i, 1
  %i.done = icmp eq i32 %i.next, %n
  br i1 %i.done, label %exit, label %outer

exit:
  ret void
}

declare float @dx.op.unary.f32(i32, float)
declare float @dx.op.waveActiveOp.f32(i32, float, i8, i8)

!dx.version = !{!0}
!dx.shaderModel = !{!1}
!dx.entryPoints = !{!2}

!0 = !{i32 1, i32 3}
!1 = !{!"lib", i32 6, i32 3}
!2 = !{null, !"", null, null, null}
