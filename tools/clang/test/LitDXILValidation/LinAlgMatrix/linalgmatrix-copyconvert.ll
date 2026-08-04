; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.LinAlgMatrixC2M5N4U1S2 = type { i8* }
%dx.types.LinAlgMatrixC4M8N4U1S2 = type { i8* }
%dx.types.LinAlgMatrixC4M5N8U1S2 = type { i8* }
%dx.types.LinAlgMatrixC4M5N4U1S2 = type { i8* }
%dx.types.LinAlgMatrixC2M4N5U1S1 = type { i8* }
%dx.types.LinAlgMatrixC2M4N5U1S0 = type { i8* }
define void @main() {
  %1 = call %dx.types.LinAlgMatrixC2M5N4U1S2 @dx.op.linAlgFillMatrix.mC2M5N4U1S2.i32(i32 -2147483636, i32 1)  ; LinAlgFillMatrix(value)

  ; CHECK: Function: main: error: Matrix Dimension '8x4' does not match expected dimension 5x4.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgCopyConvertMatrix.mC4M8N4U1S2.mC2M5N4U1S2
  %2 = call %dx.types.LinAlgMatrixC4M8N4U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M8N4U1S2.mC2M5N4U1S2(i32 -2147483635, %dx.types.LinAlgMatrixC2M5N4U1S2 %1, i1 false)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)

  ; CHECK-NEXT: Function: main: error: Matrix Dimension '5x8' does not match expected dimension 5x4.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgCopyConvertMatrix.mC4M5N8U1S2.mC2M5N4U1S2
  %3 = call %dx.types.LinAlgMatrixC4M5N8U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M5N8U1S2.mC2M5N4U1S2(i32 -2147483635, %dx.types.LinAlgMatrixC2M5N4U1S2 %1, i1 false)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)

  ; CHECK-NEXT: Function: main: error: Matrix Dimension '5x4' does not match expected dimension 4x5.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgCopyConvertMatrix.mC4M5N4U1S2.mC2M5N4U1S2
  %4 = call %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M5N4U1S2.mC2M5N4U1S2(i32 -2147483635, %dx.types.LinAlgMatrixC2M5N4U1S2 %1, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)

  ; CHECK-NEXT: Function: main: error: Matrix Scope 'Wave' does not match expected scope ThreadGroup.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgCopyConvertMatrix.mC2M4N5U1S1.mC2M5N4U1S2
  %5 = call %dx.types.LinAlgMatrixC2M4N5U1S1 @dx.op.linAlgCopyConvertMatrix.mC2M4N5U1S1.mC2M5N4U1S2(i32 -2147483635, %dx.types.LinAlgMatrixC2M5N4U1S2 %1, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)

  %6 = call %dx.types.LinAlgMatrixC2M4N5U1S0 @dx.op.linAlgFillMatrix.mC2M4N5U1S0.i32(i32 -2147483636, i32 1)  ; LinAlgFillMatrix(value)

  ; CHECK-NEXT: Function: main: error: Matrix Scope 'Thread' not allowed in LinAlgCopyConvertMatrix operation.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgCopyConvertMatrix.mC2M4N5U1S0.mC2M4N5U1S0
  %7 = call %dx.types.LinAlgMatrixC2M4N5U1S0 @dx.op.linAlgCopyConvertMatrix.mC2M4N5U1S0.mC2M4N5U1S0(i32 -2147483635, %dx.types.LinAlgMatrixC2M4N5U1S0 %6, i1 false)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC2M5N4U1S2 @dx.op.linAlgFillMatrix.mC2M5N4U1S2.i32(i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M8N4U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M8N4U1S2.mC2M5N4U1S2(i32, %dx.types.LinAlgMatrixC2M5N4U1S2, i1) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N8U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M5N8U1S2.mC2M5N4U1S2(i32, %dx.types.LinAlgMatrixC2M5N4U1S2, i1) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M5N4U1S2.mC2M5N4U1S2(i32, %dx.types.LinAlgMatrixC2M5N4U1S2, i1) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC2M4N5U1S1 @dx.op.linAlgCopyConvertMatrix.mC2M4N5U1S1.mC2M5N4U1S2(i32, %dx.types.LinAlgMatrixC2M5N4U1S2, i1) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC2M4N5U1S0 @dx.op.linAlgFillMatrix.mC2M4N5U1S0.i32(i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC2M4N5U1S0 @dx.op.linAlgCopyConvertMatrix.mC2M4N5U1S0.mC2M4N5U1S0(i32, %dx.types.LinAlgMatrixC2M4N5U1S0, i1) #0

attributes #0 = { nounwind }

!dx.targetTypes = !{!0, !1, !2, !3, !4, !5}
!llvm.ident = !{!6}
!dx.version = !{!7}
!dx.valver = !{!7}
!dx.shaderModel = !{!8}
!dx.entryPoints = !{!9}

!0 = !{%dx.types.LinAlgMatrixC2M5N4U1S2 undef, i32 2, i32 5, i32 4, i32 1, i32 2}
!1 = !{%dx.types.LinAlgMatrixC4M8N4U1S2 undef, i32 4, i32 8, i32 4, i32 1, i32 2}
!2 = !{%dx.types.LinAlgMatrixC4M5N8U1S2 undef, i32 4, i32 5, i32 8, i32 1, i32 2}
!3 = !{%dx.types.LinAlgMatrixC4M5N4U1S2 undef, i32 4, i32 5, i32 4, i32 1, i32 2}
!4 = !{%dx.types.LinAlgMatrixC2M4N5U1S1 undef, i32 2, i32 4, i32 5, i32 1, i32 1}
!5 = !{%dx.types.LinAlgMatrixC2M4N5U1S0 undef, i32 2, i32 4, i32 5, i32 1, i32 0}
!6 = !{!"dxc(private) 1.9.0.5389 (linalg-validation-component-type, b8b639b7d-dirty)"}
!7 = !{i32 1, i32 10}
!8 = !{!"cs", i32 6, i32 10}
!9 = !{void ()* @main, !"main", null, null, !10}
!10 = !{i32 4, !11}
!11 = !{i32 1, i32 1, i32 1}
