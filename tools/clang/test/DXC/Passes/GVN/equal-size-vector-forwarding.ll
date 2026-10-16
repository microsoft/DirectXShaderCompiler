; RUN: %dxopt %s -hlsl-passes-resume -basicaa -gvn -S | FileCheck %s --implicit-check-not=i128 --implicit-check-not=i256

; Forward a store to the next iteration's differently typed load without
; packing the vector into a DXIL-illegal scalar integer. Keep the load
; elimination and loop-carried PHI, using a direct vector bitcast instead.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; CHECK-LABEL: define void @loop_float4_to_uint4(
; CHECK: phi <4 x i32>
; CHECK-NOT: load <4 x i32>
; CHECK: %sum = fadd <4 x float>
; CHECK: bitcast <4 x float> %sum to <4 x i32>
; CHECK: ret void
define void @loop_float4_to_uint4(<4 x i32>* %p, <4 x float> %step,
                                 i32 %count) {
entry:
  store <4 x i32> zeroinitializer, <4 x i32>* %p
  %fp = bitcast <4 x i32>* %p to <4 x float>*
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %bits = load <4 x i32>, <4 x i32>* %p
  %value = bitcast <4 x i32> %bits to <4 x float>
  %sum = fadd <4 x float> %value, %step
  store <4 x float> %sum, <4 x float>* %fp
  %next = add i32 %i, 1
  %again = icmp ult i32 %next, %count
  br i1 %again, label %loop, label %exit

exit:
  ret void
}

; CHECK-LABEL: define void @loop_float8_to_uint8(
; CHECK: phi <8 x i32>
; CHECK-NOT: load <8 x i32>
; CHECK: %sum = fadd <8 x float>
; CHECK: bitcast <8 x float> %sum to <8 x i32>
; CHECK: ret void
define void @loop_float8_to_uint8(<8 x i32>* %p, <8 x float> %step,
                                 i32 %count) {
entry:
  store <8 x i32> zeroinitializer, <8 x i32>* %p
  %fp = bitcast <8 x i32>* %p to <8 x float>*
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %bits = load <8 x i32>, <8 x i32>* %p
  %value = bitcast <8 x i32> %bits to <8 x float>
  %sum = fadd <8 x float> %value, %step
  store <8 x float> %sum, <8 x float>* %fp
  %next = add i32 %i, 1
  %again = icmp ult i32 %next, %count
  br i1 %again, label %loop, label %exit

exit:
  ret void
}

; Also cover forwarding an integer vector store to a floating-point load.
; CHECK-LABEL: define void @loop_uint4_to_float4(
; CHECK: phi <4 x float>
; CHECK-NOT: load <4 x float>
; CHECK: %sum = add <4 x i32>
; CHECK: bitcast <4 x i32> %sum to <4 x float>
; CHECK: ret void
define void @loop_uint4_to_float4(<4 x float>* %p, <4 x i32> %step,
                                 i32 %count) {
entry:
  store <4 x float> zeroinitializer, <4 x float>* %p
  %ip = bitcast <4 x float>* %p to <4 x i32>*
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %value = load <4 x float>, <4 x float>* %p
  %bits = bitcast <4 x float> %value to <4 x i32>
  %sum = add <4 x i32> %bits, %step
  store <4 x i32> %sum, <4 x i32>* %ip
  %next = add i32 %i, 1
  %again = icmp ult i32 %next, %count
  br i1 %again, label %loop, label %exit

exit:
  ret void
}
