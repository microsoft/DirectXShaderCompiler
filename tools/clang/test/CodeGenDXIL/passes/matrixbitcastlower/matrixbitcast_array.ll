; RUN: opt -matrixbitcastlower -S %s 2>&1 | FileCheck %s

; make sure matrix bitcast lowered.
; CHECK-COUNT12: load float
; CHECK-COUNT12: store float
; CHECK-NOT:bitcast %class.matrix.float.4.3

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%class.matrix.float.4.3 = type { [4 x <3 x float>] }

define void @main() {
  %1 = alloca [24 x float], align 4
  %2 = bitcast [24 x float]* %1 to [2 x %class.matrix.float.4.3]*
  %3 = getelementptr inbounds [2 x %class.matrix.float.4.3], [2 x %class.matrix.float.4.3]* %2, i32 0, i32 1
  %4 = bitcast %class.matrix.float.4.3* %3 to <12 x float>*
  %5 = load <12 x float>, <12 x float>* %4, align 4
  store <12 x float> %5, <12 x float>* %4, align 4
  ret void
}

!dx.version = !{!3}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}

!dx.entryPoints = !{!6}


!3 = !{i32 1, i32 0}
!4 = !{i32 1, i32 7}
!5 = !{!"ps", i32 6, i32 0}
!6 = !{void ()* @main, !"main", null, null, null}

