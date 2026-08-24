; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s
target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.LinAlgMatrixC8M8N8U2S0 = type { i8* }
%dx.types.LinAlgMatrixC21M8N8U2S0 = type { i8* }
%dx.types.LinAlgMatrixC8M8N16U2S0 = type { i8* }
%dx.types.LinAlgMatrixC8M8N8U2S1 = type { i8* }
%dx.types.LinAlgMatrixC8M8N8U1S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N16U2S0 = type { i8* }

define void @main() {
  ; okay
  %1 = call %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S0.v8f16.v8f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <8 x half> <half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; okay
  %2 = call %dx.types.LinAlgMatrixC21M8N8U2S0 @dx.op.linAlgMatrixOuterProduct.mC21M8N8U2S0.v8f16.v8f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; okay
  %3 = call %dx.types.LinAlgMatrixC8M8N16U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N16U2S0.v8f16.v16f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <16 x half> <half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700, half 0xH4700>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; CHECK: Function: main: error: Return matrix scope 'Wave' does not match expected scope Thread.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S1.v8f16.v8f16
  %4 = call %dx.types.LinAlgMatrixC8M8N8U2S1 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S1.v8f16.v8f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <8 x half> <half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; CHECK-NEXT: Function: main: error: Return matrix use 'B' does not match expected use Accumulator.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixOuterProduct.mC8M8N8U1S0.v8f16.v8f16
  %5 = call %dx.types.LinAlgMatrixC8M8N8U1S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U1S0.v8f16.v8f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <8 x half> <half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; CHECK-NEXT: Function: main: error: Return matrix dimension '16x16' must match derived matrix dimension '8x8'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixOuterProduct.mC8M16N16U2S0.v8f16.v8f16
  %6 = call %dx.types.LinAlgMatrixC8M16N16U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M16N16U2S0.v8f16.v8f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <8 x half> <half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; CHECK-NEXT: Function: main: error: Return matrix dimension '8x16' must match derived matrix dimension '8x8'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixOuterProduct.mC8M8N16U2S0.v8f16.v8f16
  %7 = call %dx.types.LinAlgMatrixC8M8N16U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N16U2S0.v8f16.v8f16(i32 -2147483619, <8 x half> <half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00, half 0xH3C00>, <8 x half> <half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; CHECK-NEXT: Function: main: error: A vector element type 'half' must match B vector element type 'i32'
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S0.v8f16.v8i32
  %8 = call %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S0.v8f16.v8i32(i32 -2147483619, <8 x half> <half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000, half 0xH4000>, <8 x i32> <i32 3, i32 3, i32 3, i32 3, i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S0.v8f16.v8f16(i32, <8 x half>, <8 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC21M8N8U2S0 @dx.op.linAlgMatrixOuterProduct.mC21M8N8U2S0.v8f16.v8f16(i32, <8 x half>, <8 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N16U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N16U2S0.v8f16.v16f16(i32, <8 x half>, <16 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N8U2S1 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S1.v8f16.v8f16(i32, <8 x half>, <8 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N8U1S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U1S0.v8f16.v8f16(i32, <8 x half>, <8 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N16U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M16N16U2S0.v8f16.v8f16(i32, <8 x half>, <8 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N16U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N16U2S0.v8f16.v8f16(i32, <8 x half>, <8 x half>) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S0.v8f16.v8i32(i32, <8 x half>, <8 x i32>) #0

attributes #0 = { nounwind }

!dx.targetTypes = !{!0, !1, !2, !3, !4, !5}
!llvm.ident = !{!6}
!dx.version = !{!7}
!dx.valver = !{!7}
!dx.shaderModel = !{!8}
!dx.entryPoints = !{!9}

!0 = !{%dx.types.LinAlgMatrixC8M8N8U2S0 undef, i32 8, i32 8, i32 8, i32 2, i32 0}
!1 = !{%dx.types.LinAlgMatrixC8M8N8U2S1 undef, i32 8, i32 8, i32 8, i32 2, i32 1}
!2 = !{%dx.types.LinAlgMatrixC8M8N8U1S0 undef, i32 8, i32 8, i32 8, i32 1, i32 0}
!3 = !{%dx.types.LinAlgMatrixC8M16N16U2S0 undef, i32 8, i32 16, i32 16, i32 2, i32 0}
!4 = !{%dx.types.LinAlgMatrixC21M8N8U2S0 undef, i32 21, i32 8, i32 8, i32 2, i32 0}
!5 = !{%dx.types.LinAlgMatrixC8M8N16U2S0 undef, i32 8, i32 8, i32 16, i32 2, i32 0}
!6 = !{!"dxc(private) 1.9.0.5463 (linalg-vali-convert, c401f722f)"}
!7 = !{i32 1, i32 10}
!8 = !{!"cs", i32 6, i32 10}
!9 = !{void ()* @main, !"main", null, null, !10}
!10 = !{i32 0, i64 8388608, i32 4, !11}
!11 = !{i32 1, i32 1, i32 1}

