; RUN: %opt %s -simplifycfg -S | FileCheck %s
;
; Companion test for https://github.com/microsoft/DirectXShaderCompiler/issues/8421
;
; A switch with 33 boolean cases would naively build an `i33` bitmap (illegal
; in DXIL). A "fix" that simply rounded the bitmap width up to the next legal
; integer width would round i33 -> i64 and emit i64 lshr/trunc instructions.
; Those i64 ops cause DXIL's shader-flag analysis to set the Int64Ops feature
; flag, silently imposing a 64-bit-integer capability requirement the source
; shader did not need.
;
; The chosen fix caps the bitmap optimization at 32 bits: when the bitmap
; would need more than 32 bits, the lookup table is not built and the original
; switch is preserved. That keeps the shader free of incidental i64 operations
; and avoids adding the Int64Ops capability requirement.
;
; Verify that the original switch is preserved and no i33/i64 bitmap is built.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; CHECK-LABEL: @switch_bool_33_cases
; CHECK-NOT: i33
; CHECK-NOT: lshr i64
; CHECK-NOT: trunc i64
; CHECK: switch i32
; CHECK: ret i1

define i1 @switch_bool_33_cases(i32 %x) {
entry:
  switch i32 %x, label %default [
    i32 1,  label %case_true
    i32 2,  label %case_true
    i32 3,  label %case_false
    i32 4,  label %case_true
    i32 5,  label %case_false
    i32 6,  label %case_true
    i32 7,  label %case_true
    i32 8,  label %case_false
    i32 9,  label %case_false
    i32 10, label %case_true
    i32 11, label %case_true
    i32 12, label %case_false
    i32 13, label %case_true
    i32 14, label %case_false
    i32 15, label %case_true
    i32 16, label %case_true
    i32 17, label %case_false
    i32 18, label %case_true
    i32 19, label %case_false
    i32 20, label %case_false
    i32 21, label %case_true
    i32 22, label %case_true
    i32 23, label %case_false
    i32 24, label %case_true
    i32 25, label %case_false
    i32 26, label %case_true
    i32 27, label %case_false
    i32 28, label %case_true
    i32 29, label %case_true
    i32 30, label %case_false
    i32 31, label %case_true
    i32 32, label %case_false
    i32 33, label %case_true
  ]

case_true:
  br label %end

case_false:
  br label %end

default:
  br label %end

end:
  %result = phi i1 [ true, %case_true ], [ false, %case_false ], [ false, %default ]
  ret i1 %result
}
