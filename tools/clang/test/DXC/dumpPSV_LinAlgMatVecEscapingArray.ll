; REQUIRES: dxil-1-10
; RUN: %dxa %s -o %t.dxil | FileCheck %s --check-prefix=ASM
; RUN: %dxa %t.dxil -dumppsv | FileCheck %s

; A known RowMajor store is not sufficient when a pointer escapes to a helper
; that can overwrite the matrix with a transposed one.
; ASM: Assembly succeeded.
; CHECK: ThreadMatrixVectorMultiplyCount: 1
; CHECK: ThreadMatrixVectorMultiply[0]: ResultType=9, MatrixType=9, VectorInputType=9, Flags=3

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U0S0 = type { i8* }

define void @main() {
  %storage = alloca [2 x %dx.types.LinAlgMatrixC9M4N4U0S0]
  %element = getelementptr [2 x %dx.types.LinAlgMatrixC9M4N4U0S0], [2 x %dx.types.LinAlgMatrixC9M4N4U0S0]* %storage, i32 0, i32 0
  %matrix = call %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S0(i32 325, %dx.types.Handle undef, i32 0, i32 16, i32 0, i32 128)
  store %dx.types.LinAlgMatrixC9M4N4U0S0 %matrix, %dx.types.LinAlgMatrixC9M4N4U0S0* %element
  %alias = bitcast %dx.types.LinAlgMatrixC9M4N4U0S0* %element to i8*
  call void @OverwriteMatrix(i8* %alias)
  %loaded = load %dx.types.LinAlgMatrixC9M4N4U0S0, %dx.types.LinAlgMatrixC9M4N4U0S0* %element
  %result = call <4 x float> @dx.op.linAlgMatVecMul.v4f32.mC9M4N4U0S0.v4f32(i32 336, %dx.types.LinAlgMatrixC9M4N4U0S0 %loaded, i1 true, <4 x float> zeroinitializer, i32 9)
  ret void
}

define internal void @OverwriteMatrix(i8* %pointer) {
  %matrix = call %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S0(i32 325, %dx.types.Handle undef, i32 0, i32 0, i32 3, i32 128)
  %destination = bitcast i8* %pointer to %dx.types.LinAlgMatrixC9M4N4U0S0*
  store %dx.types.LinAlgMatrixC9M4N4U0S0 %matrix, %dx.types.LinAlgMatrixC9M4N4U0S0* %destination
  ret void
}

declare %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S0(i32, %dx.types.Handle, i32, i32, i32, i32)
declare <4 x float> @dx.op.linAlgMatVecMul.v4f32.mC9M4N4U0S0.v4f32(i32, %dx.types.LinAlgMatrixC9M4N4U0S0, i1, <4 x float>, i32)

!dx.targetTypes = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3}

!0 = !{%dx.types.LinAlgMatrixC9M4N4U0S0 undef, i32 9, i32 4, i32 4, i32 0, i32 0}
!1 = !{i32 1, i32 10}
!2 = !{!"cs", i32 6, i32 10}
!3 = !{void ()* @main, !"main", null, null, !4}
!4 = !{i32 0, i64 2199023255552, i32 4, !5}
!5 = !{i32 1, i32 1, i32 1}
