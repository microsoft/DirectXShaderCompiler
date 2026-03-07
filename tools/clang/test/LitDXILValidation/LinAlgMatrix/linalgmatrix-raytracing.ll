; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixMultiply not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixMultiply not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixMultiply not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixMultiply not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixMultiply not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixMultiply not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixAccumulate not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixAccumulate not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixAccumulate not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixAccumulate not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixAccumulate not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixAccumulate not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixLength not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixLength not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixLength not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixLength not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixLength not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixLength not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgCopyConvertMatrix not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgCopyConvertMatrix not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgCopyConvertMatrix not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgCopyConvertMatrix not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgCopyConvertMatrix not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgCopyConvertMatrix not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgFillMatrix not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgFillMatrix not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgFillMatrix not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgFillMatrix not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgFillMatrix not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgFillMatrix not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixGetElement not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixGetElement not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixGetElement not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixGetElement not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixGetElement not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixGetElement not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Opcode LinAlgMatrixSetElement not valid in shader model lib_6_10(miss).
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Opcode LinAlgMatrixSetElement not valid in shader model lib_6_10(closesthit).
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Opcode LinAlgMatrixSetElement not valid in shader model lib_6_10(anyhit).
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Opcode LinAlgMatrixSetElement not valid in shader model lib_6_10(callable).
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Opcode LinAlgMatrixSetElement not valid in shader model lib_6_10(intersection).
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Opcode LinAlgMatrixSetElement not valid in shader model lib_6_10(raygeneration).

; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  {{.*}}MainRG{{.*}}: error: Function uses features incompatible with the shader stage (raygeneration) of the entry function.

; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  {{.*}}MainIS{{.*}}: error: Function uses features incompatible with the shader stage (intersection) of the entry function.

; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  {{.*}}MainCL{{.*}}: error: Function uses features incompatible with the shader stage (callable) of the entry function.

; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  {{.*}}MainAH{{.*}}: error: Function uses features incompatible with the shader stage (anyhit) of the entry function.

; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  {{.*}}MainCH{{.*}}: error: Function uses features incompatible with the shader stage (closesthit) of the entry function.

; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  {{.*}}MainMS{{.*}}: error: Function uses features incompatible with the shader stage (miss) of the entry function.

; CHECK: Validation failed.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"
%dx.types.Handle = type { i8* }
%dx.types.LinAlgMatrixC4M5N4U2S2 = type { i8* }
%dx.types.LinAlgMatrixC4M5N4U0S2 = type { i8* }
%dx.types.LinAlgMatrixC4M4N5U1S2 = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.Attribs = type { <2 x float> }
%struct.RayPayload = type { float }
%struct.RWByteAddressBuffer = type { i32 }

@"\01?buf@@3URWByteAddressBuffer@@A" = external constant %dx.types.Handle, align 4

define void @"\01?MainRG@@YAXXZ"() #0 {
  
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A", align 4
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  ;
  ; Built-ins allowed in all stages
  ;
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon
  
  ret void
}

define void @"\01?MainIS@@YAXXZ"() #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A", align 4
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  ;
  ; Built-ins allowed in all stages
  ;
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon

  ret void
}

define void @"\01?MainCL@@YAXUAttribs@@@Z"(%struct.Attribs* noalias nocapture %attrs) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A", align 4
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  ;
  ; Built-ins allowed in all stages
  ;
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon

  ret void
}

define void @"\01?MainAH@@YAXURayPayload@@UAttribs@@@Z"(%struct.RayPayload* noalias nocapture %pld, %struct.Attribs* nocapture readnone %attrs) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A", align 4
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  ;
  ; Built-ins allowed in all stages
  ;
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon

  ret void
}

define void @"\01?MainCH@@YAXURayPayload@@UAttribs@@@Z"(%struct.RayPayload* noalias nocapture %pld, %struct.Attribs* nocapture readnone %attrs) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A", align 4
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  ;
  ; Built-ins allowed in all stages
  ;
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon

  ret void
}

define void @"\01?MainMS@@YAXURayPayload@@@Z"(%struct.RayPayload* noalias nocapture %pld) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A", align 4
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  ;
  ; Built-ins allowed in all stages
  ;
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon

  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, %dx.types.LinAlgMatrixC4M4N5U1S2) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, %dx.types.LinAlgMatrixC4M4N5U1S2) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, %dx.types.Handle, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, %dx.types.Handle, i32, i32, i32) #0

; Function Attrs: nounwind
declare i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32, %dx.types.Handle, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32, <4 x i32>, <4 x i32>) #0

; Function Attrs: nounwind
declare i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32) #0

; Function Attrs: nounwind
declare <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, <4 x i32>, i32) #0

; Function Attrs: nounwind
declare <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, <4 x i32>, i32, <4 x i32>, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, i1) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32, i32) #0

; Function Attrs: nounwind
declare <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, i32) #0

; Function Attrs: nounwind
declare float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, %dx.types.LinAlgMatrixC4M4N5U1S2, %dx.types.LinAlgMatrixC4M5N4U2S2) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32, %dx.types.LinAlgMatrixC4M5N4U0S2, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #1

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32, %dx.types.Handle) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!dx.targetTypes = !{!0, !1, !2}
!llvm.ident = !{!3}
!dx.version = !{!4}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}
!dx.resources = !{!6}
!dx.typeAnnotations = !{!9}
!dx.dxrPayloadAnnotations = !{!17}
!dx.entryPoints = !{!20, !22, !25, !27, !29, !31, !33}

!0 = !{%dx.types.LinAlgMatrixC4M5N4U0S2 undef, i32 4, i32 5, i32 4, i32 0, i32 2}
!1 = !{%dx.types.LinAlgMatrixC4M4N5U1S2 undef, i32 4, i32 4, i32 5, i32 1, i32 2}
!2 = !{%dx.types.LinAlgMatrixC4M5N4U2S2 undef, i32 4, i32 5, i32 4, i32 2, i32 2}
!3 = !{!"dxc(private) 1.9.0.15241 (Main, 1f63535ae)"}
!4 = !{i32 1, i32 10}
!5 = !{!"lib", i32 6, i32 10}
!6 = !{null, !7, null, null}
!7 = !{!8}
!8 = !{i32 0, %struct.RWByteAddressBuffer* bitcast (%dx.types.Handle* @"\01?buf@@3URWByteAddressBuffer@@A" to %struct.RWByteAddressBuffer*), !"buf", i32 -1, i32 -1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!9 = !{i32 1, void ()* @"\01?MainRG@@YAXXZ", !10, void ()* @"\01?MainIS@@YAXXZ", !10, void (%struct.Attribs*)* @"\01?MainCL@@YAXUAttribs@@@Z", !13, void (%struct.RayPayload*, %struct.Attribs*)* @"\01?MainAH@@YAXURayPayload@@UAttribs@@@Z", !15, void (%struct.RayPayload*, %struct.Attribs*)* @"\01?MainCH@@YAXURayPayload@@UAttribs@@@Z", !15, void (%struct.RayPayload*)* @"\01?MainMS@@YAXURayPayload@@@Z", !13}
!10 = !{!11}
!11 = !{i32 1, !12, !12}
!12 = !{}
!13 = !{!11, !14}
!14 = !{i32 2, !12, !12}
!15 = !{!11, !14, !16}
!16 = !{i32 0, !12, !12}
!17 = !{i32 0, %struct.RayPayload undef, !18}
!18 = !{!19}
!19 = !{i32 0, i32 13107}
!20 = !{null, !"", null, !6, !21}
!21 = !{i32 0, i64 8589934608}
!22 = !{void ()* @"\01?MainRG@@YAXXZ", !"\01?MainRG@@YAXXZ", null, null, !23}
!23 = !{i32 8, i32 7, i32 5, !24}
!24 = !{i32 0}
!25 = !{void (%struct.RayPayload*, %struct.Attribs*)* @"\01?MainAH@@YAXURayPayload@@UAttribs@@@Z", !"\01?MainAH@@YAXURayPayload@@UAttribs@@@Z", null, null, !26}
!26 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !24}
!27 = !{void (%struct.Attribs*)* @"\01?MainCL@@YAXUAttribs@@@Z", !"\01?MainCL@@YAXUAttribs@@@Z", null, null, !28}
!28 = !{i32 8, i32 12, i32 6, i32 8, i32 5, !24}
!29 = !{void (%struct.RayPayload*, %struct.Attribs*)* @"\01?MainCH@@YAXURayPayload@@UAttribs@@@Z", !"\01?MainCH@@YAXURayPayload@@UAttribs@@@Z", null, null, !30}
!30 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !24}
!31 = !{void ()* @"\01?MainIS@@YAXXZ", !"\01?MainIS@@YAXXZ", null, null, !32}
!32 = !{i32 8, i32 8, i32 5, !24}
!33 = !{void (%struct.RayPayload*)* @"\01?MainMS@@YAXURayPayload@@@Z", !"\01?MainMS@@YAXURayPayload@@@Z", null, null, !34}
!34 = !{i32 8, i32 11, i32 6, i32 4, i32 5, !24}
