; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixMultiply not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixAccumulate not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixStoreToDescriptor not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixLength not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgCopyConvertMatrix not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgFillMatrix not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixGetCoordinate not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixGetElement not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixMultiplyAccumulate not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Opcode LinAlgMatrixSetElement not valid in shader model hs_6_10.
; CHECK: Function:  MainHS: error: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
; CHECK: Function:  MainHS: error: Function uses features incompatible with the shader stage (hs) of the entry function.
; CHECK: Validation failed.


target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.LinAlgMatrixC4M5N4U2S2 = type { i8* }
%dx.types.LinAlgMatrixC4M5N4U0S2 = type { i8* }
%dx.types.LinAlgMatrixC4M4N5U1S2 = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.RWByteAddressBuffer = type { i32 }

define void @MainHS() {

  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

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

  ret void
}

define void @"\01?HSPatch@@YA?AUPCStruct@@V?$InputPatch@UPosStruct@@$02@@V?$OutputPatch@UPosStruct@@$02@@I@Z"() {
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 0, i32 0, i8 0, float %1)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 0, i32 1, i8 0, float %1)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 0, i32 2, i8 0, float %1)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 1, i32 0, i8 0, float %1)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 2, i32 0, i8 0, float undef)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 2, i32 0, i8 1, float undef)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 2, i32 0, i8 2, float undef)  ; StorePatchConstant(outputSigID,row,col,value)
  call void @dx.op.storePatchConstant.f32(i32 106, i32 2, i32 0, i8 3, float undef)  ; StorePatchConstant(outputSigID,row,col,value)
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
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storePatchConstant.f32(i32, i32, i32, i8, float) #1

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
!5 = !{!"hs", i32 6, i32 10}
!6 = !{null, !7, null, null}
!7 = !{!8}
!8 = !{i32 0, %struct.RWByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!9 = !{[11 x i32] [i32 4, i32 4, i32 1, i32 2, i32 4, i32 8, i32 20, i32 0, i32 0, i32 0, i32 6280]}
!10 = !{void ()* @MainHS, !"MainHS", !11, !6, !22}
!11 = !{!12, !12, !16}
!12 = !{!13}
!13 = !{i32 0, !"SV_Position", i8 9, i8 3, !14, i8 4, i32 1, i8 4, i32 0, i8 0, !15}
!14 = !{i32 0}
!15 = !{i32 3, i32 15}
!16 = !{!17, !20, !21}
!17 = !{i32 0, !"SV_TessFactor", i8 9, i8 25, !18, i8 0, i32 3, i8 1, i32 0, i8 3, !19}
!18 = !{i32 0, i32 1, i32 2}
!19 = !{i32 3, i32 1}
!20 = !{i32 1, !"SV_InsideTessFactor", i8 9, i8 26, !14, i8 0, i32 1, i8 1, i32 3, i8 0, !19}
!21 = !{i32 2, !"TEST", i8 9, i8 0, !14, i8 0, i32 1, i8 4, i32 4, i8 0, !15}
!22 = !{i32 0, i64 8590000144, i32 3, !23}
!23 = !{void ()* @"\01?HSPatch@@YA?AUPCStruct@@V?$InputPatch@UPosStruct@@$02@@V?$OutputPatch@UPosStruct@@$02@@I@Z", i32 3, i32 3, i32 2, i32 3, i32 3, float 6.400000e+01}
