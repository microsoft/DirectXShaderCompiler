; REQUIRES: dxil-1-10
; RUN: %dxv %s 2>&1 | FileCheck %s

; The groupshared memory operand of the LinAlgMatrix memory operations may be a
; pointer to a vector of a legal component type.

; CHECK: Validation succeeded.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.LinAlgMatrixC9M4N4U1S1 = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U2S1 = type { i8* }

@"\01?SharedVecArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [64 x <4 x float>], align 4

define void @main() {
  %1 = call %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v4f32(i32 -2147483633, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 0, i32 16, i32 1)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v4f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M4N4U1S1 %1, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 0, i32 16, i32 1)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)
  %2 = call %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix.mC9M4N4U2S1.i32(i32 -2147483636, i32 0)  ; LinAlgFillMatrix(value)
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.v4f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %2, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 9, i32 0, i32 16, i32 1)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v4f32(i32, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v4f32(i32, %dx.types.LinAlgMatrixC9M4N4U1S1, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix.mC9M4N4U2S1.i32(i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.v4f32(i32, %dx.types.LinAlgMatrixC9M4N4U2S1, <4 x float> addrspace(3)*, i32, i32, i32, i32) #0

attributes #0 = { nounwind }

!dx.targetTypes = !{!0, !1}
!llvm.ident = !{!2}
!dx.version = !{!3}
!dx.valver = !{!3}
!dx.shaderModel = !{!4}
!dx.entryPoints = !{!5}

!0 = !{%dx.types.LinAlgMatrixC9M4N4U1S1 undef, i32 9, i32 4, i32 4, i32 1, i32 1}
!1 = !{%dx.types.LinAlgMatrixC9M4N4U2S1 undef, i32 9, i32 4, i32 4, i32 2, i32 1}
!2 = !{!"dxc(private) 1.9.0.15454 (main, 2ad500cb4)"}
!3 = !{i32 1, i32 10}
!4 = !{!"cs", i32 6, i32 10}
!5 = !{void ()* @main, !"main", null, null, !6}
!6 = !{i32 4, !7}
!7 = !{i32 4, i32 4, i32 4}
