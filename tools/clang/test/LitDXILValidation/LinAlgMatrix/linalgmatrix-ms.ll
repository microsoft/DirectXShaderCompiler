; REQUIRES: dxil-1-10
; RUN: %dxv %s 2>&1 | FileCheck %s

; CHECK: Validation succeeded.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.LinAlgMatrixC4M5N4U2S2 = type { i8* }
%dx.types.LinAlgMatrixC4M5N4U0S2 = type { i8* }
%dx.types.LinAlgMatrixC4M4N5U1S2 = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.RWByteAddressBuffer = type { i32 }

define void @mainMeS() {

  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  %thread_id_group = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()
  ;
  ; Built-ins allowed in all stages
  ;

  ; dx.op.linAlgMatrixAccumulate
  %v1 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483624, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.LinAlgMatrixC4M4N5U1S2 undef)  ; LinAlgMatrixAccumulate(matrixLHS,matrixRHS)
  
  ; dx.op.linAlgMatrixAccumulateToDescriptor
  call void @dx.op.linAlgMatrixAccumulateToDescriptor.mC4M5N4U0S2(i32 -2147483621, %dx.types.LinAlgMatrixC4M5N4U0S2 undef, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixAccumulateToDescriptor(matrix,handle,offset,stride,layout)
  
  ; dx.op.linAlgMatrixLength
  %v2 = call i32 @dx.op.linAlgMatrixLength.mC4M5N4U0S2(i32 -2147483632, %dx.types.LinAlgMatrixC4M5N4U0S2 undef)  ; LinAlgMatrixLength(matrix)
  
  ; dx.op.linAlgMatrixLoadFromDescriptor
  %v3 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M5N4U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 5, i32 5, i32 5)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  
  ; dx.op.linAlgMatrixOuterProduct
  %v4 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixOuterProduct.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483619, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, <4 x i32> <i32 3, i32 3, i32 3, i32 3>)  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
 
  ; dx.op.linAlgMatrixQueryAccumulatorLayout
  %v5 = call i32 @dx.op.linAlgMatrixQueryAccumulatorLayout(i32 -2147483626)  ; LinAlgMatrixQueryAccumulatorLayout()
  
  ; dx.op.linAlgMatVecMul
  %v6 = call <4 x i32> @dx.op.linAlgMatVecMul.v4i32.mC4M5N4U0S2.v4i32(i32 -2147483623, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 1)  ; LinAlgMatVecMul(matrix,inputVector,interpretation)
  
  ; dx.op.linAlgMatVecMulAdd
  %v7 = call <4 x i32> @dx.op.linAlgMatVecMulAdd.v4i32.mC4M5N4U0S2.v4i32.v4i32(i32 -2147483622, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, <4 x i32> <i32 9, i32 9, i32 9, i32 9>, i32 2, <4 x i32> <i32 7, i32 7, i32 7, i32 7>, i32 3)  ; LinAlgMatVecMulAdd(matrix,inputVector,inputInterpretation,biasVector,biasInterpretation)
  
  ;
  ; Built-ins restricted to compute, mesh and amplification shaders
  ;

  ; dx.op.linAlgCopyConvertMatrix
  %v8 = call %dx.types.LinAlgMatrixC4M4N5U1S2 @dx.op.linAlgCopyConvertMatrix.mC4M4N5U1S2.mC4M5N4U0S2(i32 -2147483635, %dx.types.LinAlgMatrixC4M5N4U0S2 %v4, i1 true)  ; LinAlgCopyConvertMatrix(srcMatrix,transpose)
 
  ; dx.op.linAlgFillMatrix
  %v9 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgFillMatrix.mC4M5N4U0S2.i32(i32 -2147483636, i32 15)  ; LinAlgFillMatrix(value)
  
  ; dx.op.linAlgMatrixGetCoordinate
  %v10 = call <2 x i32> @dx.op.linAlgMatrixGetCoordinate.mC4M5N4U0S2(i32 -2147483631, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetCoordinate(matrix,threadLocalIndex)
  
  ; dx.op.linAlgMatrixGetElement
  %v11 = call float @dx.op.linAlgMatrixGetElement.f32.mC4M5N4U0S2(i32 -2147483630, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 0)  ; LinAlgMatrixGetElement(matrix,threadLocalIndex)
  
  ; dx.op.linAlgMatrixMultiply
  %v12 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiply.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2(i32 -2147483625, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8)  ; LinAlgMatrixMultiply(matrixA,matrixB)
  
  ; dx.op.linAlgMatrixMultiplyAccumulate
  %v13 = call %dx.types.LinAlgMatrixC4M5N4U2S2 @dx.op.linAlgMatrixMultiplyAccumulate.mC4M5N4U2S2.mC4M5N4U0S2.mC4M4N5U1S2.mC4M5N4U2S2(i32 -2147483637, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, %dx.types.LinAlgMatrixC4M4N5U1S2 %v8, %dx.types.LinAlgMatrixC4M5N4U2S2 %v12)  ; LinAlgMatrixMultiplyAccumulate(matrixA,matrixB,matrixC)
  
  ; dx.op.linAlgMatrixSetElement
  %v14 = call %dx.types.LinAlgMatrixC4M5N4U0S2 @dx.op.linAlgMatrixSetElement.mC4M5N4U0S2.mC4M5N4U0S2.i32(i32 -2147483629, %dx.types.LinAlgMatrixC4M5N4U0S2 %v9, i32 1, i32 1)  ; LinAlgMatrixSetElement(matrix,threadLocalIndex,value)

  ; dx.op.linAlgMatrixStoreToDescriptor
  call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U0S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U0S2 %v14, %dx.types.Handle %handle, i32 1, i32 2, i32 3)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  
  ; FIXME: 3 more ops coming soon

  call void @dx.op.setMeshOutputCounts(i32 168, i32 32, i32 16)  ; SetMeshOutputCounts(numVertices,numPrimitives)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 0, float 0.000000e+00, i32 %thread_id_group)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 1, float 0.000000e+00, i32 %thread_id_group)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 2, float 0.000000e+00, i32 %thread_id_group)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 3, float 0.000000e+00, i32 %thread_id_group)  ; StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)
  
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

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeVertexOutput.f32(i32, i32, i32, i8, float, i32) #1

; Function Attrs: nounwind
declare void @dx.op.setMeshOutputCounts(i32, i32, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!dx.targetTypes = !{!0, !1, !2}
!llvm.ident = !{!3}
!dx.version = !{!4}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}
!dx.resources = !{!6}
!dx.viewIdState = !{!9}
!dx.entryPoints = !{!10}

!0 = !{%dx.types.LinAlgMatrixC4M5N4U0S2 undef, i32 4, i32 5, i32 4, i32 0, i32 2}
!1 = !{%dx.types.LinAlgMatrixC4M4N5U1S2 undef, i32 4, i32 4, i32 5, i32 1, i32 2}
!2 = !{%dx.types.LinAlgMatrixC4M5N4U2S2 undef, i32 4, i32 5, i32 4, i32 2, i32 2}
!3 = !{!"dxc(private) 1.9.0.15241 (main, 1f63535ae)"}
!4 = !{i32 1, i32 10}
!5 = !{!"ms", i32 6, i32 10}
!6 = !{null, !7, null, null}
!7 = !{!8}
!8 = !{i32 0, %struct.RWByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!9 = !{[3 x i32] [i32 0, i32 4, i32 0]}
!10 = !{void ()* @mainMeS, !"mainMeS", !11, !6, !16}
!11 = !{null, !12, null}
!12 = !{!13}
!13 = !{i32 0, !"SV_Position", i8 9, i8 3, !14, i8 4, i32 1, i8 4, i32 0, i8 0, !15}
!14 = !{i32 0}
!15 = !{i32 3, i32 15}
!16 = !{i32 0, i64 8589934608, i32 9, !17}
!17 = !{!18, i32 32, i32 0, i32 2, i32 0}
!18 = !{i32 8, i32 8, i32 2}
