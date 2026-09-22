; RUN: %dxopt %s -hlsl-passes-resume -scalarrepl-param-hlsl -S | FileCheck %s

; Scalar replacement splits an array of vectors into one scalar array per
; vector lane. Verify that invalid local lane indices are not scalarized.
; Constant global GEPs are first canonicalized across the entire aggregate,
; turning lane 7 into array index 1, lane 3, which can be scalarized safely.

; CHECK-DAG: %valid.3 = alloca [2 x float]
; CHECK-DAG: %nested = alloca [2 x %struct.S]
; CHECK-DAG: %out_of_bounds = alloca [2 x <4 x float>]
; CHECK-DAG: %negative = alloca [2 x <4 x float>]
; CHECK-DAG: @global_out_of_bounds.3 = internal global [2 x float] zeroinitializer
; CHECK: getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %out_of_bounds, i32 0, i32 0, i32 7
; CHECK: getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %negative, i32 0, i32 0, i32 -1
; CHECK: getelementptr inbounds [2 x float], [2 x float]* %valid.3, i32 0, i32 0
; CHECK: getelementptr [2 x %struct.S], [2 x %struct.S]* %nested, i32 0, i32 0, i32 0, i32 7
; CHECK: getelementptr inbounds ([2 x float], [2 x float]* @global_out_of_bounds.3, i32 0, i64 1)

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%ConstantBuffer = type opaque
%struct.S = type { <4 x float> }

@"$Globals" = external constant %ConstantBuffer
@global_out_of_bounds = internal global [2 x <4 x float>] zeroinitializer

define <4 x float> @main() {
entry:
  %out_of_bounds = alloca [2 x <4 x float>]
  %out_of_bounds.vector = getelementptr [2 x <4 x float>], [2 x <4 x float>]* %out_of_bounds, i32 0, i32 0
  %out_of_bounds.element = getelementptr <4 x float>, <4 x float>* %out_of_bounds.vector, i32 0, i32 7
  store float 9.000000e+00, float* %out_of_bounds.element

  %negative = alloca [2 x <4 x float>]
  %negative.vector = getelementptr [2 x <4 x float>], [2 x <4 x float>]* %negative, i32 0, i32 0
  %negative.element = getelementptr <4 x float>, <4 x float>* %negative.vector, i32 0, i32 -1
  store float 9.000000e+00, float* %negative.element

  %valid = alloca [2 x <4 x float>]
  %valid.vector = getelementptr [2 x <4 x float>], [2 x <4 x float>]* %valid, i32 0, i32 0
  %valid.element = getelementptr <4 x float>, <4 x float>* %valid.vector, i32 0, i32 3
  store float 9.000000e+00, float* %valid.element

  %nested = alloca [2 x %struct.S]
  %nested.element = getelementptr [2 x %struct.S], [2 x %struct.S]* %nested, i32 0, i32 0, i32 0, i32 7
  store float 9.000000e+00, float* %nested.element

  store float 9.000000e+00, float* getelementptr inbounds ([2 x <4 x float>], [2 x <4 x float>]* @global_out_of_bounds, i32 0, i32 0, i32 7)

  ret <4 x float> zeroinitializer
}

!pauseresume = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4}
!dx.entryPoints = !{!8}
!dx.fnprops = !{!12}
!dx.options = !{!13, !14}

!0 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!1 = !{i32 1, i32 6}
!2 = !{i32 1, i32 10}
!3 = !{!"ps", i32 6, i32 6}
!4 = !{i32 1, <4 x float> ()* @main, !5}
!5 = !{!6}
!6 = !{i32 1, !7, !7}
!7 = !{}
!8 = !{<4 x float> ()* @main, !"main", null, !9, null}
!9 = !{null, null, !10, null}
!10 = !{!11}
!11 = !{i32 0, %ConstantBuffer* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 0, null}
!12 = !{<4 x float> ()* @main, i32 0, i1 false}
!13 = !{i32 64}
!14 = !{i32 -1}
