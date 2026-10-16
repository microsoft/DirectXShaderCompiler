; RUN: opt -S -basicaa -gvn -verify %s -o %t
; RUN: FileCheck %s < %t
; RUN: FileCheck %s --check-prefix=WIDTH < %t

; Use natural i8/i16 alignment so padding checks do not mask illegal integer
; widths below 64 bits. A maximum-width check alone would allow i24 and i48.
;
; WIDTH: target triple = "dxil-ms-dx"
; WIDTH-NOT: {{i(24|48|96|128|192|256)}}

target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; Reject an illegal source-width intermediate even with a legal result width.
;
; CHECK-LABEL: @store_24_to_8(
; CHECK: %result = load i8, i8* %cast, align 1
; CHECK-NEXT: ret i8 %result
define i8 @store_24_to_8(<3 x i8> %value, <3 x i8>* %ptr) {
entry:
  store <3 x i8> %value, <3 x i8>* %ptr, align 1
  %cast = bitcast <3 x i8>* %ptr to i8*
  %result = load i8, i8* %cast, align 1
  ret i8 %result
}

; CHECK-LABEL: @load_48_to_16(
; CHECK: %value = load <3 x i16>, <3 x i16>* %ptr, align 2
; CHECK: %reload = load i16, i16* %cast, align 2
; CHECK-NEXT: %result = insertelement <3 x i16> %value, i16 %reload, i32 2
; CHECK-NEXT: ret <3 x i16> %result
define <3 x i16> @load_48_to_16(<3 x i16>* %ptr) {
entry:
  %value = load <3 x i16>, <3 x i16>* %ptr, align 2
  %cast = bitcast <3 x i16>* %ptr to i16*
  %reload = load i16, i16* %cast, align 2
  %result = insertelement <3 x i16> %value, i16 %reload, i32 2
  ret <3 x i16> %result
}

; Legal scalar integer sources must not be truncated to illegal widths either.
;
; CHECK-LABEL: @store_32_to_24(
; CHECK: %result = load <3 x i8>, <3 x i8>* %cast, align 1
; CHECK-NEXT: ret <3 x i8> %result
define <3 x i8> @store_32_to_24(i32 %value, i32* %ptr) {
entry:
  store i32 %value, i32* %ptr, align 4
  %cast = bitcast i32* %ptr to <3 x i8>*
  %result = load <3 x i8>, <3 x i8>* %cast, align 1
  ret <3 x i8> %result
}

; CHECK-LABEL: @store_64_to_48(
; CHECK: %result = load <3 x i16>, <3 x i16>* %cast, align 2
; CHECK-NEXT: ret <3 x i16> %result
define <3 x i16> @store_64_to_48(i64 %value, i64* %ptr) {
entry:
  store i64 %value, i64* %ptr, align 8
  %cast = bitcast i64* %ptr to <3 x i16>*
  %result = load <3 x i16>, <3 x i16>* %cast, align 2
  ret <3 x i16> %result
}

; Nonlocal availability must reject an illegal destination-width intermediate.
;
; CHECK-LABEL: @store_64_to_48_nonlocal(
; CHECK: exit:
; CHECK-NEXT: %result = load <3 x i16>, <3 x i16>* %cast, align 2
; CHECK-NEXT: ret <3 x i16> %result
define <3 x i16> @store_64_to_48_nonlocal(i64 %a, i64 %b, i64* %ptr,
                                        i1 %condition) {
entry:
  %cast = bitcast i64* %ptr to <3 x i16>*
  br i1 %condition, label %left, label %right

left:
  store i64 %a, i64* %ptr, align 8
  br label %exit

right:
  store i64 %b, i64* %ptr, align 8
  br label %exit

exit:
  %result = load <3 x i16>, <3 x i16>* %cast, align 2
  ret <3 x i16> %result
}

; An equal-sized 48-bit vector-to-vector bitcast needs no scalar integer.
;
; CHECK-LABEL: @equal_size_48(
; CHECK: %[[CAST:.*]] = bitcast <3 x i16> %value to <6 x i8>
; CHECK-NOT: load
; CHECK: ret <6 x i8> %[[CAST]]
define <6 x i8> @equal_size_48(<3 x i16> %value, <3 x i16>* %ptr) {
entry:
  store <3 x i16> %value, <3 x i16>* %ptr, align 2
  %cast = bitcast <3 x i16>* %ptr to <6 x i8>*
  %result = load <6 x i8>, <6 x i8>* %cast, align 2
  ret <6 x i8> %result
}

; Legal small-width extraction must still be optimized.
;
; CHECK-LABEL: @legal_16_to_8(
; CHECK: %[[LOW:.*]] = trunc i16 %value to i8
; CHECK-NEXT: ret i8 %[[LOW]]
define i8 @legal_16_to_8(i16 %value, i16* %ptr) {
entry:
  store i16 %value, i16* %ptr, align 2
  %cast = bitcast i16* %ptr to i8*
  %result = load i8, i8* %cast, align 1
  ret i8 %result
}

; Nonlocal load forwarding uses the same legality checks as store forwarding.
;
; CHECK-LABEL: @load_64_to_48_nonlocal(
; CHECK: exit:
; CHECK: %result = load <3 x i16>, <3 x i16>* %cast, align 2
; CHECK: %pair = insertvalue { i64, <3 x i16> } %with.source, <3 x i16> %result, 1
; CHECK-NEXT: ret { i64, <3 x i16> } %pair
define { i64, <3 x i16> } @load_64_to_48_nonlocal(i64* %ptr, i1 %condition) {
entry:
  %cast = bitcast i64* %ptr to <3 x i16>*
  br i1 %condition, label %left, label %right

left:
  %a = load i64, i64* %ptr, align 8
  br label %exit

right:
  %b = load i64, i64* %ptr, align 8
  br label %exit

exit:
  %source = phi i64 [ %a, %left ], [ %b, %right ]
  %result = load <3 x i16>, <3 x i16>* %cast, align 2
  %with.source = insertvalue { i64, <3 x i16> } undef, i64 %source, 0
  %pair = insertvalue { i64, <3 x i16> } %with.source, <3 x i16> %result, 1
  ret { i64, <3 x i16> } %pair
}

; Equal-sized nonlocal loads still need only a vector bitcast.
;
; CHECK-LABEL: @equal_size_48_load_nonlocal(
; CHECK: left:
; CHECK: %[[LEFT:.*]] = bitcast <3 x i16> %a to <6 x i8>
; CHECK: right:
; CHECK: %[[RIGHT:.*]] = bitcast <3 x i16> %b to <6 x i8>
; CHECK: exit:
; CHECK: %result = phi <6 x i8> [ %[[LEFT]], %left ], [ %[[RIGHT]], %right ]
; CHECK-NOT: load
; CHECK: %pair = insertvalue { <3 x i16>, <6 x i8> } %with.source, <6 x i8> %result, 1
; CHECK-NEXT: ret { <3 x i16>, <6 x i8> } %pair
define { <3 x i16>, <6 x i8> } @equal_size_48_load_nonlocal(
    <3 x i16>* %ptr, i1 %condition) {
entry:
  %cast = bitcast <3 x i16>* %ptr to <6 x i8>*
  br i1 %condition, label %left, label %right

left:
  %a = load <3 x i16>, <3 x i16>* %ptr, align 2
  br label %exit

right:
  %b = load <3 x i16>, <3 x i16>* %ptr, align 2
  br label %exit

exit:
  %source = phi <3 x i16> [ %a, %left ], [ %b, %right ]
  %result = load <6 x i8>, <6 x i8>* %cast, align 2
  %with.source = insertvalue { <3 x i16>, <6 x i8> } undef, <3 x i16> %source, 0
  %pair = insertvalue { <3 x i16>, <6 x i8> } %with.source, <6 x i8> %result, 1
  ret { <3 x i16>, <6 x i8> } %pair
}

; Equal byte sizes do not imply equal bit sizes: i8 to i1 still needs a trunc.
;
; CHECK-LABEL: @legal_8_to_1_nonlocal(
; CHECK: left:
; CHECK: %[[LEFT:.*]] = trunc i8 %a to i1
; CHECK: right:
; CHECK: %[[RIGHT:.*]] = trunc i8 %b to i1
; CHECK: exit:
; CHECK-NEXT: %result = phi i1 [ %[[RIGHT]], %right ], [ %[[LEFT]], %left ]
; CHECK-NEXT: ret i1 %result
define i1 @legal_8_to_1_nonlocal(i8 %a, i8 %b, i8* %ptr, i1 %condition) {
entry:
  %cast = bitcast i8* %ptr to i1*
  br i1 %condition, label %left, label %right

left:
  store i8 %a, i8* %ptr, align 1
  br label %exit

right:
  store i8 %b, i8* %ptr, align 1
  br label %exit

exit:
  %result = load i1, i1* %cast, align 1
  ret i1 %result
}
