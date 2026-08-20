; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.LinAlgMatrixC9M4N4U0S0 = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U0S1 = type { i8* }
%dx.types.LinAlgMatrixC6M4N4U0S2 = type { i8* }
%dx.types.LinAlgMatrixC9M8N8U0S1 = type { i8* }
%dx.types.LinAlgMatrixC9M9N8U0S1 = type { i8* }
%dx.types.LinAlgMatrixC9M8N9U0S1 = type { i8* }
%struct.ByteAddressBuffer = type { i32 }

@"\01?SharedArr@@3PAMA" = external addrspace(3) global [64 x float], align 4
@"\01?SharedVecArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [16 x <4 x float>], align 4

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %3 = call %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S0(i32 -2147483634, %dx.types.Handle %2, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %4 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %5 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S1(i32 -2147483634, %dx.types.Handle %4, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %6 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %7 = call %dx.types.LinAlgMatrixC6M4N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC6M4N4U0S2(i32 -2147483634, %dx.types.Handle %6, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %9 = call %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N8U0S1(i32 -2147483634, %dx.types.Handle %8, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %10 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %11 = call %dx.types.LinAlgMatrixC9M9N8U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M9N8U0S1(i32 -2147483634, %dx.types.Handle %10, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %12 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %13 = call %dx.types.LinAlgMatrixC9M8N9U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N9U0S1(i32 -2147483634, %dx.types.Handle %12, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)

  ; okay
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S1.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M4N4U0S1 %5, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK: Function: main: error: parameter 'Offset' must be a multiple of 128, got 129
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S1.f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S1.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M4N4U0S1 %5, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 129, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: parameter 'Stride' must be a multiple of 16, got 17
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S1.f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S1.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M4N4U0S1 %5, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 17, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Input matrix scope 'Thread' does not match expected scope Wave or ThreadGroup.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S0.f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S0.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M4N4U0S0 %3, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory inner type 'float' must match input matrix type 'I64'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC6M4N4U0S2.f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC6M4N4U0S2.f32(i32 -2147483627, %dx.types.LinAlgMatrixC6M4N4U0S2 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; okay
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M8N8U0S1.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M8N8U0S1 %9, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M9N8U0S1.f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M9N8U0S1.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M9N8U0S1 %11, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M8N9U0S1.f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M8N9U0S1.f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M8N9U0S1 %13, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; okay
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M8N8U0S1.v4f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M8N8U0S1 %9, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M9N8U0S1.v4f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M9N8U0S1.v4f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M9N8U0S1 %11, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixStoreToMemory.mC9M8N9U0S1.v4f32
  call void @dx.op.linAlgMatrixStoreToMemory.mC9M8N9U0S1.v4f32(i32 -2147483627, %dx.types.LinAlgMatrixC9M8N9U0S1 %13, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixStoreToMemory(matrix,memory,offset,stride,layout)

  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC6M4N4U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC6M4N4U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N8U0S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M9N8U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M9N8U0S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N9U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N9U0S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S1.f32(i32, %dx.types.LinAlgMatrixC9M4N4U0S1, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M4N4U0S0.f32(i32, %dx.types.LinAlgMatrixC9M4N4U0S0, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC6M4N4U0S2.f32(i32, %dx.types.LinAlgMatrixC6M4N4U0S2, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M8N8U0S1.f32(i32, %dx.types.LinAlgMatrixC9M8N8U0S1, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M9N8U0S1.f32(i32, %dx.types.LinAlgMatrixC9M9N8U0S1, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M8N9U0S1.f32(i32, %dx.types.LinAlgMatrixC9M8N9U0S1, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M8N8U0S1.v4f32(i32, %dx.types.LinAlgMatrixC9M8N8U0S1, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M9N8U0S1.v4f32(i32, %dx.types.LinAlgMatrixC9M9N8U0S1, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixStoreToMemory.mC9M8N9U0S1.v4f32(i32, %dx.types.LinAlgMatrixC9M8N9U0S1, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!dx.targetTypes = !{!0, !1, !2, !3, !4, !5}
!llvm.ident = !{!6}
!dx.version = !{!7}
!dx.valver = !{!7}
!dx.shaderModel = !{!8}
!dx.resources = !{!9}
!dx.entryPoints = !{!12}

!0 = !{%dx.types.LinAlgMatrixC9M4N4U0S0 undef, i32 9, i32 4, i32 4, i32 0, i32 0}
!1 = !{%dx.types.LinAlgMatrixC9M4N4U0S1 undef, i32 9, i32 4, i32 4, i32 0, i32 1}
!2 = !{%dx.types.LinAlgMatrixC6M4N4U0S2 undef, i32 6, i32 4, i32 4, i32 0, i32 2}
!3 = !{%dx.types.LinAlgMatrixC9M8N8U0S1 undef, i32 9, i32 8, i32 8, i32 0, i32 1}
!4 = !{%dx.types.LinAlgMatrixC9M9N8U0S1 undef, i32 9, i32 9, i32 8, i32 0, i32 1}
!5 = !{%dx.types.LinAlgMatrixC9M8N9U0S1 undef, i32 9, i32 8, i32 9, i32 0, i32 1}
!6 = !{!"dxc(private) 1.9.0.5466 (linalg-vali-matrixstoretomemory, 07bef449f-dirty)"}
!7 = !{i32 1, i32 10}
!8 = !{!"cs", i32 6, i32 10}
!9 = !{!10, null, null, null}
!10 = !{!11}
!11 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!12 = !{void ()* @main, !"main", null, !9, !13}
!13 = !{i32 0, i64 8388624, i32 4, !14}
!14 = !{i32 1, i32 1, i32 1}

