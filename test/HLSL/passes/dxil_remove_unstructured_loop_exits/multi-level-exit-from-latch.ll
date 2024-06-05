; RUN: opt %s -analyze -loops | FileCheck -check-prefix=LOOPBEFORE %s
; RUN: opt %s -dxil-remove-unstructured-loop-exits -o %t.bc
; RUN: opt %t.bc -S | FileCheck %s
; RUN: opt %t.bc -analyze -loops | FileCheck -check-prefix=LOOPAFTER %s

; The exiting edge from the latch block of the loop at depth 3 exits to the loop at depth 1.
; This reproduces the original bug.
;
; Loop exits are 'dedicated', one of the LoopSimplifyForm criteria.

;
;   entry
;    |
;    v
;   header.1 --> header.2 --> header.3 --> if.3 -----> exiting.3
;    ^            ^            ^            |           |  |
;    |            |            |            v           |  |
;    |            |           latch.3 <--- endif.3 <----+  |
;    |            |            |                           |
;    |            |            |                           v
;    |           latch.2 <----------------------------- exit.3.to.2
;    |            |            |
;    | +-------- latch.2.exit  |
;    | |                       |
;    | |                       v
;    | |                      latch.3.exit
;    | |                       |
;    | v                       |
;   latch.1  <-----------------+
;    |
;    v
;   end
;


; LOOPBEFORE:      Loop at depth 1 containing: %header.1<header>,%header.2,%header.3,%if.3,%exiting.3,%endif.3,%latch.3,%latch.3.exit,%exit.3.to.2,%latch.2,%latch.2.exit,%latch.1<latch><exiting>
; LOOPBEFORE-NEXT: Loop at depth 2 containing: %header.2<header>,%header.3,%if.3,%exiting.3,%endif.3,%latch.3<exiting>,%exit.3.to.2,%latch.2<latch><exiting>
; LOOPBEFORE-NEXT: Loop at depth 3 containing: %header.3<header>,%if.3,%exiting.3<exiting>,%endif.3,%latch.3<latch><exiting>
; no more loops expected
; LOOPBEFORE-NOT:  Loop at depth

; LOOPAFTER:      Loop at depth 1 containing: %header.1<header>,%header.2,%header.3,%if.3,%exiting.3,%dx.struct_exit.new_exiting,%endif.3,%latch.3,%latch.3.exit,%0,%exit.3.to.2,%dx.struct_exit.new_exiting4,%latch.2,%latch.2.exit,%1,%latch.3.exit.split,%latch.1<latch><exiting>
; LOOPAFTER-NEXT: Loop at depth 2 containing: %header.2<header>,%header.3,%if.3,%exiting.3,%dx.struct_exit.new_exiting,%endif.3,%latch.3,%latch.3.exit,%0,%exit.3.to.2,%dx.struct_exit.new_exiting4<exiting>,%latch.2<latch><exiting>
; LOOPAFTER-NEXT: Loop at depth 3 containing: %header.3<header>,%if.3,%exiting.3,%dx.struct_exit.new_exiting<exiting>,%endif.3,%latch.3<latch><exiting>
; no more loops expected
; LOOPAFTER-NOT:  Loop at depth


target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"


define void @main(i1 %cond) {
entry:
  br label %header.1

header.1:
  br label %header.2

header.2:
  br label %header.3

header.3:
  br label %if.3

if.3:
  br i1 %cond, label %exiting.3, label %endif.3

exiting.3:
  %x3val = add i32 0, 0
  br i1 %cond, label %exit.3.to.2, label %endif.3

endif.3:
  br label %latch.3

latch.3:
  br i1 %cond, label %latch.3.exit, label %header.3

latch.3.exit:
  br label %latch.1

latch.2:
  %l2val = phi i32 [ %x3val, %exit.3.to.2 ]
  br i1 %cond, label %latch.2.exit, label %header.2

latch.2.exit:
  br label %latch.1

exit.3.to.2:
  br label %latch.2

latch.1:
  br i1 %cond, label %end, label %header.1

end:
 ret void
}


; CHECK: define void @main(i1 %cond) {
; CHECK: entry:
; CHECK:   br label %header.1

; CHECK: header.1:
; CHECK:   br label %header.2

; CHECK: header.2:
; CHECK:   br label %header.3

; CHECK: header.3:
; CHECK:   br label %if.3

; CHECK: if.3:
; CHECK:   br i1 %cond, label %exiting.3, label %dx.struct_exit.new_exiting

; CHECK: exiting.3:
; CHECK:   %x3val = add i32 0, 0
; CHECK:   br label %dx.struct_exit.new_exiting

; CHECK: dx.struct_exit.new_exiting:
; CHECK:   %dx.struct_exit.prop1 = phi i1 [ %cond, %exiting.3 ], [ false, %if.3 ]
; CHECK:   %dx.struct_exit.prop = phi i32 [ %x3val, %exiting.3 ], [ 0, %if.3 ]
; CHECK:   br i1 %dx.struct_exit.prop1, label %latch.3.exit, label %endif.3

; CHECK: endif.3:
; CHECK:   br label %latch.3

; CHECK: latch.3:
; CHECK:   br i1 %cond, label %latch.3.exit, label %header.3

; CHECK: latch.3.exit:
; CHECK:   %dx.struct_exit.exit_cond_lcssa = phi i1 [ %dx.struct_exit.prop1, %dx.struct_exit.new_exiting ], [ false, %latch.3 ]
; CHECK:   %dx.struct_exit.val_lcssa = phi i32 [ %dx.struct_exit.prop, %dx.struct_exit.new_exiting ], [ 0, %latch.3 ]
; CHECK:   br i1 %dx.struct_exit.exit_cond_lcssa, label %exit.3.to.2, label %0

; CHECK: <label>:0
; CHECK:   br label %dx.struct_exit.new_exiting4

; CHECK: latch.3.exit.split:
; CHECK:   br label %latch.1

; CHECK: dx.struct_exit.new_exiting4:
; CHECK:   %dx.struct_exit.prop3 = phi i1 [ true, %0 ], [ false, %exit.3.to.2 ]
; CHECK:   %l2val = phi i32 [ %x3val.lcssa, %exit.3.to.2 ], [ 0, %0 ]
; CHECK:   br i1 %dx.struct_exit.prop3, label %latch.2.exit, label %latch.2

; CHECK: latch.2:
; CHECK:   br i1 %cond, label %latch.2.exit, label %header.2

; CHECK: latch.2.exit:
; CHECK:   %dx.struct_exit.exit_cond_lcssa6 = phi i1 [ %dx.struct_exit.prop3, %dx.struct_exit.new_exiting4 ], [ false, %latch.2 ]
; CHECK:   br i1 %dx.struct_exit.exit_cond_lcssa6, label %latch.3.exit.split, label %1

; CHECK: <label>:1
; CHECK:   br label %latch.1

; CHECK: exit.3.to.2:
; CHECK:   %x3val.lcssa = phi i32 [ %dx.struct_exit.val_lcssa, %latch.3.exit ]
; CHECK:   br label %dx.struct_exit.new_exiting4

; CHECK: latch.1:
; CHECK:   br i1 %cond, label %end, label %header.1

; CHECK: end:
; CHECK:   ret void
; CHECK: }
