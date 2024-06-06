; RUN: opt %s -analyze -loops | FileCheck -check-prefix=LOOPBEFORE %s
; RUN: opt %s -dxil-remove-unstructured-loop-exits -o %t.bc
; RUN: opt %t.bc -S | FileCheck %s
; RUN: opt %t.bc -analyze -loops | FileCheck -check-prefix=LOOPAFTER %s

; The exiting edge goes to the header of a completely unrelated loop.
; That edge is 'critical' in CFG terms, and will be split before attempting
; to restructure the exit.


;   entry
;    |                        +---------+
;    v                        v         |
;   header.1 --> if.1 -----> header.u2 -+
;    ^            |           |
;    |            |           |
;    | +-------- endif.1     end.u2
;    | |
;    | v
;   latch.1
;    |
;    v
;   end
;

; LOOPBEFORE-DAG:  Loop at depth 1 containing: %header.u2<header><latch><exiting>
; LOOPBEFORE-DAG:  Loop at depth 1 containing: %header.1<header>,%if.1<exiting>,%endif.1,%latch.1<latch><exiting>
; LOOPBEFORE-NOT:  Loop at depth

; LOOPAFTER-DAG:   Loop at depth 1 containing: %header.u2<header><latch><exiting>
; LOOPAFTER-DAG:   Loop at depth 1 containing: %header.1<header>,%if.1<exiting>,%endif.1,%latch.1<latch><exiting>
; LOOPAFTER-NOT:  Loop at depth


target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"


define void @main(i1 %cond) {
entry:
  br label %header.1

header.1:
  br label %if.1

if.1:
  br i1 %cond, label %header.u2, label %endif.1

endif.1:
  br label %latch.1

latch.1:
  br i1 %cond, label %end, label %header.1

end:
 ret void

header.u2:
  br i1 %cond, label %end.u2, label %header.u2

end.u2:
 ret void
}


; CHECK: define void @main
; CHECK: entry:
; CHECK:   br label %header.1

; CHECK: header.1:
; CHECK:   br label %if.1

; CHECK: if.1:
; CHECK:   br i1 %cond, label %if.1.header.u2_crit_edge, label %endif.1

; CHECK: if.1.header.u2_crit_edge:
; CHECK:   br label %header.u2

; CHECK: endif.1:
; CHECK:   br label %latch.1

; CHECK: latch.1:
; CHECK:   br i1 %cond, label %end, label %header.1

; CHECK: end:
; CHECK:   ret void

; CHECK: header.u2:
; CHECK:   br i1 %cond, label %end.u2, label %header.u2

; CHECK: end.u2:
; CHECK:   ret void
; CHECK: }
