; RUN: opt -matrixbitcastlower -S %s 2>&1 | FileCheck %s

; make sure matrix bitcast not lowered.
; CHECK:bitcast %class.matrix.float.4.3


target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%class.matrix.float.4.3 = type { [4 x <3 x float>] }

define void @foo(float *) {
  ret void
}

define void @bar(i32 * ) {
  ret void
}

define void @main() {
  %1 = alloca [12 x float], align 4
  %2 = bitcast [12 x float]* %1 to %class.matrix.float.4.3*
  %3 = getelementptr inbounds %class.matrix.float.4.3, %class.matrix.float.4.3* %2, i32 0
  %4 = bitcast %class.matrix.float.4.3* %3 to <12 x float>*
  %5 = getelementptr <12 x float>, <12 x float>* %4, i32 0, i32 0
  call void @foo(float *%5)
  %6 = bitcast float* %5 to i32 *
  call void @bar(i32 * %6)
  %7 = load <12 x float>, <12 x float>* %4, align 4
  store <12 x float> %7, <12 x float>* %4, align 4
  ret void
}

!dx.version = !{!3}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}

!dx.entryPoints = !{!6}


!3 = !{i32 1, i32 0}
!4 = !{i32 1, i32 7}
!5 = !{!"lib", i32 6, i32 3}
!6 = !{void ()* @main, !"main", null, null, null}

