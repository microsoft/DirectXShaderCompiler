; RUN: opt -S -basicaa -gvn -verify %s -o %t
; RUN: FileCheck %s < %t
; RUN: FileCheck %s --check-prefix=WIDTH < %t

; Check the whole output, including instructions between the per-function checks.
; WIDTH: target triple = "dxil-ms-dx"
; WIDTH-NOT: {{i(24|48|96|128|192|256)}}

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; GVN must not coerce the dynamically indexed store through i96, which is not
; a legal integer type in DXIL.
;
; CHECK-LABEL: @dynamic_index(
; CHECK-NOT: i96
; CHECK: %[[INITIAL:.*]] = bitcast <3 x i32> %initial to <3 x float>
; CHECK: %result = phi <3 x float> [ %result.pre, %store ], [ %[[INITIAL]], %entry ]
; CHECK: ret <3 x float> %result
define <3 x float> @dynamic_index(<3 x i32> %initial,
                                  <3 x i32> %conditional, i32 %index) {
entry:
  %values = alloca [2 x <3 x float>], align 4
  %first = getelementptr inbounds [2 x <3 x float>],
                                  [2 x <3 x float>]* %values, i32 0, i32 0
  %first.i32 = bitcast <3 x float>* %first to <3 x i32>*
  store <3 x i32> %initial, <3 x i32>* %first.i32, align 4
  %cmp = icmp ugt i32 %index, 1
  br i1 %cmp, label %store, label %exit

store:
  %array.index = and i32 %index, 1
  %element = getelementptr inbounds [2 x <3 x float>],
                                    [2 x <3 x float>]* %values, i32 0,
                                    i32 %array.index
  %element.i32 = bitcast <3 x float>* %element to <3 x i32>*
  store <3 x i32> %conditional, <3 x i32>* %element.i32, align 4
  br label %exit

exit:
  %reload = getelementptr inbounds [2 x <3 x float>],
                                   [2 x <3 x float>]* %values, i32 0, i32 0
  %result = load <3 x float>, <3 x float>* %reload, align 4
  ret <3 x float> %result
}

; GVN cannot extract this load at a nonzero offset without creating illegal
; i128 and i96 intermediates, so it must leave the load in place.
;
; CHECK-LABEL: @nonzero_offset(
; CHECK-NOT: i128
; CHECK-NOT: i96
; CHECK: %result = load <3 x float>, <3 x float>* %vector, align 4
; CHECK: ret <3 x float> %result
define <3 x float> @nonzero_offset(<4 x float> %value, <4 x float>* %ptr) {
entry:
  store <4 x float> %value, <4 x float>* %ptr, align 4
  %bytes = bitcast <4 x float>* %ptr to i8*
  %offset = getelementptr inbounds i8, i8* %bytes, i32 4
  %vector = bitcast i8* %offset to <3 x float>*
  %result = load <3 x float>, <3 x float>* %vector, align 4
  ret <3 x float> %result
}

; Equal-sized wide vectors can be forwarded without a scalar integer
; intermediate, both locally and when available values are merged with a phi.
;
; CHECK-LABEL: @equal_size_local(
; CHECK: %[[CAST:.*]] = bitcast <3 x i32> %value to <3 x float>
; CHECK-NOT: load
; CHECK: ret <3 x float> %[[CAST]]
define <3 x float> @equal_size_local(<3 x i32> %value, <3 x i32>* %ptr) {
entry:
  store <3 x i32> %value, <3 x i32>* %ptr, align 4
  %cast = bitcast <3 x i32>* %ptr to <3 x float>*
  %result = load <3 x float>, <3 x float>* %cast, align 4
  ret <3 x float> %result
}

; CHECK-LABEL: @equal_size_nonlocal(
; CHECK: left:
; CHECK: %[[LEFT:.*]] = bitcast <3 x i32> %a to <3 x float>
; CHECK: right:
; CHECK: %[[RIGHT:.*]] = bitcast <3 x i32> %b to <3 x float>
; CHECK: exit:
; CHECK: %result = phi <3 x float> [ %[[RIGHT]], %right ], [ %[[LEFT]], %left ]
; CHECK-NEXT: ret <3 x float> %result
define <3 x float> @equal_size_nonlocal(<3 x i32> %a, <3 x i32> %b,
                                       <3 x i32>* %ptr, i1 %condition) {
entry:
  %cast = bitcast <3 x i32>* %ptr to <3 x float>*
  br i1 %condition, label %left, label %right

left:
  store <3 x i32> %a, <3 x i32>* %ptr, align 4
  br label %exit

right:
  store <3 x i32> %b, <3 x i32>* %ptr, align 4
  br label %exit

exit:
  %result = load <3 x float>, <3 x float>* %cast, align 4
  ret <3 x float> %result
}

; Load-to-load forwarding also supports equal-sized vectors wider than 64 bits.
; Use 192 bits here to avoid only testing the original 96-bit size.
;
; CHECK-LABEL: @equal_size_load(
; CHECK: %value = load <3 x double>, <3 x double>* %ptr, align 8
; CHECK-NOT: load
; CHECK: %[[CAST:.*]] = bitcast <3 x double> %value to <6 x i32>
; CHECK-NEXT: %result = insertvalue { <3 x double>, <6 x i32> } undef, <3 x double> %value, 0
; CHECK-NEXT: %pair = insertvalue { <3 x double>, <6 x i32> } %result, <6 x i32> %[[CAST]], 1
; CHECK-NEXT: ret { <3 x double>, <6 x i32> } %pair
define { <3 x double>, <6 x i32> } @equal_size_load(<3 x double>* %ptr) {
entry:
  %value = load <3 x double>, <3 x double>* %ptr, align 8
  %cast = bitcast <3 x double>* %ptr to <6 x i32>*
  %reload = load <6 x i32>, <6 x i32>* %cast, align 8
  %result = insertvalue { <3 x double>, <6 x i32> } undef, <3 x double> %value, 0
  %pair = insertvalue { <3 x double>, <6 x i32> } %result, <6 x i32> %reload, 1
  ret { <3 x double>, <6 x i32> } %pair
}

; Even when the result is a legal i32, extracting it from a wide vector must
; not create an i96 intermediate. Cover both store and load definitions.
;
; CHECK-LABEL: @wide_store_to_scalar(
; CHECK: store <3 x i32> %value, <3 x i32>* %ptr, align 4
; CHECK: %result = load i32, i32* %cast, align 4
; CHECK-NEXT: ret i32 %result
define i32 @wide_store_to_scalar(<3 x i32> %value, <3 x i32>* %ptr) {
entry:
  store <3 x i32> %value, <3 x i32>* %ptr, align 4
  %cast = bitcast <3 x i32>* %ptr to i32*
  %result = load i32, i32* %cast, align 4
  ret i32 %result
}

; CHECK-LABEL: @wide_load_to_scalar(
; CHECK: %value = load <3 x i32>, <3 x i32>* %ptr, align 4
; CHECK: %reload = load i32, i32* %cast, align 4
; CHECK-NEXT: %result = insertelement <3 x i32> %value, i32 %reload, i32 2
; CHECK-NEXT: ret <3 x i32> %result
define <3 x i32> @wide_load_to_scalar(<3 x i32>* %ptr) {
entry:
  %value = load <3 x i32>, <3 x i32>* %ptr, align 4
  %cast = bitcast <3 x i32>* %ptr to i32*
  %reload = load i32, i32* %cast, align 4
  %result = insertelement <3 x i32> %value, i32 %reload, i32 2
  ret <3 x i32> %result
}

; The destination width matters too: extracting a three-lane vector from a
; four-lane vector would create both i128 and i96, even at offset zero.
;
; CHECK-LABEL: @wide_store_to_vector(
; CHECK: %result = load <3 x float>, <3 x float>* %cast, align 4
; CHECK-NEXT: ret <3 x float> %result
define <3 x float> @wide_store_to_vector(<4 x float> %value, <4 x float>* %ptr) {
entry:
  store <4 x float> %value, <4 x float>* %ptr, align 4
  %cast = bitcast <4 x float>* %ptr to <3 x float>*
  %result = load <3 x float>, <3 x float>* %cast, align 4
  ret <3 x float> %result
}

; Nonlocal availability analysis must reject unsafe coercions before trying to
; materialize the incoming values for a phi.
;
; CHECK-LABEL: @wide_store_nonlocal(
; CHECK: exit:
; CHECK-NEXT: %result = load i32, i32* %cast, align 4
; CHECK-NEXT: ret i32 %result
define i32 @wide_store_nonlocal(<3 x i32> %a, <3 x i32> %b,
                                <3 x i32>* %ptr, i1 %condition) {
entry:
  %cast = bitcast <3 x i32>* %ptr to i32*
  br i1 %condition, label %left, label %right

left:
  store <3 x i32> %a, <3 x i32>* %ptr, align 4
  br label %exit

right:
  store <3 x i32> %b, <3 x i32>* %ptr, align 4
  br label %exit

exit:
  %result = load i32, i32* %cast, align 4
  ret i32 %result
}

; A 64-bit source and a 32-bit destination need only legal integer types, so
; the width checks must not disable this optimization.
;
; CHECK-LABEL: @legal_partial_store(
; CHECK: %[[BITS:.*]] = bitcast <2 x i32> %value to i64
; CHECK-NEXT: %[[LOW:.*]] = trunc i64 %[[BITS]] to i32
; CHECK-NEXT: %[[CAST:.*]] = bitcast i32 %[[LOW]] to float
; CHECK-NEXT: ret float %[[CAST]]
define float @legal_partial_store(<2 x i32> %value, <2 x i32>* %ptr) {
entry:
  store <2 x i32> %value, <2 x i32>* %ptr, align 4
  %cast = bitcast <2 x i32>* %ptr to float*
  %result = load float, float* %cast, align 4
  ret float %result
}
