; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.LinAlgMatrixC9M4N4U2S0 = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U0S1 = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U2S1 = type { i8* }
%dx.types.LinAlgMatrixC6M4N4U2S2 = type { i8* }
%dx.types.LinAlgMatrixC9M8N8U2S1 = type { i8* }
%dx.types.LinAlgMatrixC9M9N8U2S1 = type { i8* }
%dx.types.LinAlgMatrixC9M8N9U2S1 = type { i8* }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%struct.ByteAddressBuffer = type { i32 }

@"\01?SharedArr@@3PAMA" = external addrspace(3) global [64 x float], align 4
@"\01?SharedVecArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [16 x <4 x float>], align 4
@"\01?SharedIntArr@@3PAIA" = external addrspace(3) global [64 x i32], align 4

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %3 = call %dx.types.LinAlgMatrixC9M4N4U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U2S0(i32 -2147483634, %dx.types.Handle %2, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %4 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %5 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S1(i32 -2147483634, %dx.types.Handle %4, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %6 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %7 = call %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U2S1(i32 -2147483634, %dx.types.Handle %6, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %9 = call %dx.types.LinAlgMatrixC6M4N4U2S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC6M4N4U2S2(i32 -2147483634, %dx.types.Handle %8, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %10 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %11 = call %dx.types.LinAlgMatrixC9M8N8U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N8U2S1(i32 -2147483634, %dx.types.Handle %10, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %12 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %13 = call %dx.types.LinAlgMatrixC9M9N8U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M9N8U2S1(i32 -2147483634, %dx.types.Handle %12, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %14 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %15 = call %dx.types.LinAlgMatrixC9M8N9U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N9U2S1(i32 -2147483634, %dx.types.Handle %14, i32 0, i32 0, i32 0, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)

  ; okay
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK: Function: main: error: Parameter 'Offset' in bytes must be a multiple of 128, got 516 (129 elements * 4 bytes per element).
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 129, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; okay only if offset accounts for byte width of component type
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 32, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Parameter 'Stride' in bytes must be a multiple of 16, got 68 (17 elements * 4 bytes per element).
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 17, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; okay only if stride accounts for byte width of component type
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 4, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory inner type 'float' must match target type 'I64'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 6, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  %16 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %17 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %16, i32 0, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %18 = extractvalue %dx.types.ResRet.i32 %17, 0

  ; CHECK-NEXT: Function: main: error: TargetType of LinAlgMatrixAccumulateToMemory must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 %18, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Component type 'Invalid' from TargetType not allowed in LinAlg Matrix operations.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.i32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.i32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S1 %7, i32 addrspace(3)* getelementptr inbounds ([64 x i32], [64 x i32] addrspace(3)* @"\01?SharedIntArr@@3PAIA", i32 0, i32 0), i32 0, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Input matrix scope 'Thread' does not match expected scope Wave or ThreadGroup.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S0.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S0.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U2S0 %3, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Input matrix use 'A' does not match expected use Accumulator.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U0S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U0S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M4N4U0S1 %5, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory inner type 'float' must match input matrix type 'I64'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC6M4N4U2S2.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC6M4N4U2S2.f32(i32 -2147483620, %dx.types.LinAlgMatrixC6M4N4U2S2 %9, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; okay
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N8U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M8N8U2S1 %11, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M9N8U2S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M9N8U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M9N8U2S1 %13, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N9U2S1.f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N9U2S1.f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M8N9U2S1 %15, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; okay
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N8U2S1.v4f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M8N8U2S1 %11, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M9N8U2S1.v4f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M9N8U2S1.v4f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M9N8U2S1 %13, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N9U2S1.v4f32
  call void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N9U2S1.v4f32(i32 -2147483620, %dx.types.LinAlgMatrixC9M8N9U2S1 %15, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 9, i32 128, i32 16, i32 0)  ; LinAlgMatrixAccumulateToMemory(matrix,memory,targetType,offset,stride,layout)

  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U2S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U0S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M4N4U2S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC6M4N4U2S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC6M4N4U2S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N8U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N8U2S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M9N8U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M9N8U2S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N9U2S1 @dx.op.linAlgMatrixLoadFromDescriptor.mC9M8N9U2S1(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.f32(i32, %dx.types.LinAlgMatrixC9M4N4U2S1, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S1.i32(i32, %dx.types.LinAlgMatrixC9M4N4U2S1, i32 addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U2S0.f32(i32, %dx.types.LinAlgMatrixC9M4N4U2S0, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M4N4U0S1.f32(i32, %dx.types.LinAlgMatrixC9M4N4U0S1, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC6M4N4U2S2.f32(i32, %dx.types.LinAlgMatrixC6M4N4U2S2, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N8U2S1.f32(i32, %dx.types.LinAlgMatrixC9M8N8U2S1, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M9N8U2S1.f32(i32, %dx.types.LinAlgMatrixC9M9N8U2S1, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N9U2S1.f32(i32, %dx.types.LinAlgMatrixC9M8N9U2S1, float addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N8U2S1.v4f32(i32, %dx.types.LinAlgMatrixC9M8N8U2S1, <4 x float> addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M9N8U2S1.v4f32(i32, %dx.types.LinAlgMatrixC9M9N8U2S1, <4 x float> addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.linAlgMatrixAccumulateToMemory.mC9M8N9U2S1.v4f32(i32, %dx.types.LinAlgMatrixC9M8N9U2S1, <4 x float> addrspace(3)*, i32, i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone }

!dx.targetTypes = !{!0, !1, !2, !3, !4, !5, !6}
!llvm.ident = !{!7}
!dx.version = !{!8}
!dx.valver = !{!8}
!dx.shaderModel = !{!9}
!dx.resources = !{!10}
!dx.entryPoints = !{!13}

!0 = !{%dx.types.LinAlgMatrixC9M4N4U2S0 undef, i32 9, i32 4, i32 4, i32 2, i32 0}
!1 = !{%dx.types.LinAlgMatrixC9M4N4U0S1 undef, i32 9, i32 4, i32 4, i32 0, i32 1}
!2 = !{%dx.types.LinAlgMatrixC9M4N4U2S1 undef, i32 9, i32 4, i32 4, i32 2, i32 1}
!3 = !{%dx.types.LinAlgMatrixC6M4N4U2S2 undef, i32 6, i32 4, i32 4, i32 2, i32 2}
!4 = !{%dx.types.LinAlgMatrixC9M8N8U2S1 undef, i32 9, i32 8, i32 8, i32 2, i32 1}
!5 = !{%dx.types.LinAlgMatrixC9M9N8U2S1 undef, i32 9, i32 9, i32 8, i32 2, i32 1}
!6 = !{%dx.types.LinAlgMatrixC9M8N9U2S1 undef, i32 9, i32 8, i32 9, i32 2, i32 1}
!7 = !{!"dxc(private) 1.9.0.5466 (linalg-vali-matrixstoretomemory, 6c8ce1341-dirty)"}
!8 = !{i32 1, i32 10}
!9 = !{!"cs", i32 6, i32 10}
!10 = !{!11, null, null, null}
!11 = !{!12}
!12 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!13 = !{void ()* @main, !"main", null, !10, !14}
!14 = !{i32 0, i64 2199031644176, i32 4, !15}
!15 = !{i32 1, i32 1, i32 1}
