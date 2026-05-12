; RUN: %opt %s -simplifycfg -S | FileCheck %s
;
; Regression test for https://github.com/microsoft/DirectXShaderCompiler/issues/8421
;
; SimplifyCFG's switch-to-lookup-table built bitmaps of width
; "TableSize * ValueBitWidth", producing illegal widths like i9, i17, i26 or
; i40. The fix rounds the bitmap up to i16 or i32.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; 5 boolean cases with mixed values would naively build an i5 bitmap; rounded
; to i16 by the fix.
;
; CHECK-LABEL: @switch_bool_5_cases
; CHECK: switch.lookup:
; CHECK: lshr i16
; CHECK-NOT: i5
; CHECK: ret i1

define i1 @switch_bool_5_cases(i32 %x) {
entry:
  switch i32 %x, label %default [
    i32 0, label %case_t
    i32 1, label %case_t
    i32 2, label %case_f
    i32 3, label %case_t
    i32 4, label %case_f
  ]

case_t:
  br label %end

case_f:
  br label %end

default:
  br label %end

end:
  %result = phi i1 [ true, %case_t ], [ false, %case_f ], [ false, %default ]
  ret i1 %result
}

; 9 boolean cases would naively build an i9 bitmap; rounded to i16 by the fix.
;
; CHECK-LABEL: @switch_bool_9_cases
; CHECK: switch.lookup:
; CHECK: lshr i16
; CHECK-NOT: i9
; CHECK: ret i1

define i1 @switch_bool_9_cases(i32 %x) {
entry:
  switch i32 %x, label %default [
    i32 0, label %case_true
    i32 1, label %case_true
    i32 3, label %case_true
    i32 4, label %case_true
    i32 5, label %case_true
    i32 6, label %case_true
    i32 7, label %case_true
    i32 8, label %case_true
  ]

case_true:
  br label %end

default:
  br label %end

end:
  %result = phi i1 [ true, %case_true ], [ false, %default ]
  ret i1 %result
}

; 17 boolean cases would naively build an i17 bitmap; rounded to i32 by the
; fix.
;
; CHECK-LABEL: @switch_bool_17_cases
; CHECK: switch.lookup:
; CHECK: lshr i32
; CHECK-NOT: i17
; CHECK: ret i1

define i1 @switch_bool_17_cases(i32 %x) {
entry:
  switch i32 %x, label %default [
    i32 0,  label %case_true
    i32 1,  label %case_true
    i32 3,  label %case_true
    i32 4,  label %case_true
    i32 5,  label %case_true
    i32 7,  label %case_true
    i32 8,  label %case_true
    i32 10, label %case_true
    i32 11, label %case_true
    i32 12, label %case_true
    i32 14, label %case_true
    i32 15, label %case_true
    i32 16, label %case_true
  ]

case_true:
  br label %end

default:
  br label %end

end:
  %result = phi i1 [ true, %case_true ], [ false, %default ]
  ret i1 %result
}

; 26 boolean cases (the original #8421 repro) would naively build an i26
; bitmap; rounded to i32 by the fix.
;
; CHECK-LABEL: @switch_bool_26_cases
; CHECK: switch.lookup:
; CHECK: lshr i32
; CHECK-NOT: i26
; CHECK: ret i1

define i1 @switch_bool_26_cases(i32 %x) {
entry:
  switch i32 %x, label %default [
    i32 1, label %case_true
    i32 6, label %case_true
    i32 11, label %case_true
    i32 16, label %case_true
    i32 21, label %case_true
    i32 26, label %case_true
  ]

case_true:
  br label %end

default:
  br label %end

end:
  %result = phi i1 [ true, %case_true ], [ false, %default ]
  ret i1 %result
}

; Non-i1 result: 5 i8 cases would naively build an i40 bitmap. The fix makes
; bitmaps wider than 32 bits fall back to an array (or preserve the switch).
;
; CHECK-LABEL: @switch_i8_5_cases
; CHECK-NOT: i40
; CHECK: ret i8

define i8 @switch_i8_5_cases(i32 %x) {
entry:
  switch i32 %x, label %default [
    i32 0, label %c0
    i32 1, label %c1
    i32 2, label %c2
    i32 3, label %c3
    i32 4, label %c4
  ]

c0: br label %end
c1: br label %end
c2: br label %end
c3: br label %end
c4: br label %end
default: br label %end

end:
  ; Non-linear values prevent the LinearMap fast path so the bitmap path is
  ; the one that would have been chosen.
  %result = phi i8 [73, %c0], [42, %c1], [19, %c2], [88, %c3], [31, %c4], [0, %default]
  ret i8 %result
}
