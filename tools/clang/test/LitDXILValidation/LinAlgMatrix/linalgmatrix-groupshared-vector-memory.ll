; REQUIRES: dxil-1-10
; RUN: %dxv %s 2>&1 | FileCheck %s

; The groupshared memory operand of the LinAlgMatrix memory operations may be a
; pointer to a vector of a legal component type.

; CHECK: Validation succeeded.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.LinAlgMatrixC9M4N4U1S1 = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U2S1 = type { i8* }

@"\01?SharedVecArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [64 x <4 x float>], align 4
@SharedI16 = external addrspace(3) global [32 x i16], align 4
@SharedI64Vec = external addrspace(3) global [4 x <2 x i64>], align 8

define void @main() {
  %1 = call %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v4f32(i32 326, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 0, i32 16, i32 1)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v4f32(i32 332, %dx.types.LinAlgMatrixC9M4N4U1S1 %1, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 0, i32 16, i32 1)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)
  %2 = call %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix.mC9M4N4U2S1.i32(i32 323, i1 true, i32 0)  ; LinAlgFillMatrix(isInputSigned,value)
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.v4f32(i32 339, %dx.types.LinAlgMatrixC9M4N4U2S1 %2, <4 x float> addrspace(3)* getelementptr inbounds ([64 x <4 x float>], [64 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 0, i32 16, i32 1)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,offset,stride,layout)
  ; Integer storage is unconverted, and each array holds exactly 64 bytes.
  %3 = call %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.i16(i32 326, i16 addrspace(3)* getelementptr inbounds ([32 x i16], [32 x i16] addrspace(3)* @SharedI16, i32 0, i32 0), i32 0, i32 4, i32 0)
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.i16(i32 332, %dx.types.LinAlgMatrixC9M4N4U1S1 %3, i16 addrspace(3)* getelementptr inbounds ([32 x i16], [32 x i16] addrspace(3)* @SharedI16, i32 0, i32 0), i32 0, i32 4, i32 0)
  %4 = call %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v2i64(i32 326, <2 x i64> addrspace(3)* getelementptr inbounds ([4 x <2 x i64>], [4 x <2 x i64>] addrspace(3)* @SharedI64Vec, i32 0, i32 0), i32 0, i32 4, i32 1)
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v2i64(i32 332, %dx.types.LinAlgMatrixC9M4N4U1S1 %4, <2 x i64> addrspace(3)* getelementptr inbounds ([4 x <2 x i64>], [4 x <2 x i64>] addrspace(3)* @SharedI64Vec, i32 0, i32 0), i32 0, i32 4, i32 1)
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v4f32(i32, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v4f32(i32, %dx.types.LinAlgMatrixC9M4N4U1S1, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix.mC9M4N4U2S1.i32(i32, i1, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.v4f32(i32, %dx.types.LinAlgMatrixC9M4N4U2S1, <4 x float> addrspace(3)*, i32, i32, i32) #0

declare %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.i16(i32, i16 addrspace(3)*, i32, i32, i32) #0
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.i16(i32, %dx.types.LinAlgMatrixC9M4N4U1S1, i16 addrspace(3)*, i32, i32, i32) #0
declare %dx.types.LinAlgMatrixC9M4N4U1S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U1S1.v2i64(i32, <2 x i64> addrspace(3)*, i32, i32, i32) #0
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U1S1.v2i64(i32, %dx.types.LinAlgMatrixC9M4N4U1S1, <2 x i64> addrspace(3)*, i32, i32, i32) #0

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
!6 = !{i32 0, i64 2199031644160, i32 4, !7}
!7 = !{i32 4, i32 4, i32 4}
