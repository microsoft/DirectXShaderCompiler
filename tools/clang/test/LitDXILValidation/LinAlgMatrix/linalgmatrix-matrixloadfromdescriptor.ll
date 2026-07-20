; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.LinAlgMatrixC8M4N4U2S2 = type { i8* }
%dx.types.LinAlgMatrixC8M4N4U2S0 = type { i8* }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%dx.types.LinAlgMatrixC8M8N8U2S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N16U2S0 = type { i8* }
%dx.types.LinAlgMatrixC8M4N8U2S0 = type { i8* }
%struct.ByteAddressBuffer = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer

  ; CHECK: Function: main: error: Matrix scope 'ThreadGroup' requires layout RowMajor or ColumnMajor.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S2
  %3 = call %dx.types.LinAlgMatrixC8M4N4U2S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S2(i32 -2147483634, %dx.types.Handle %2, i32 0, i32 0, i32 4, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %4 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  ; CHECK-NEXT: Function: main: error: Matrix layout 'OuterProductOptimal' requires stride 0.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S0
  %5 = call %dx.types.LinAlgMatrixC8M4N4U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S0(i32 -2147483634, %dx.types.Handle %4, i32 0, i32 4, i32 4, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %6 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %7 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %6, i32 0, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %8 = extractvalue %dx.types.ResRet.i32 %7, 0
  ; CHECK-NEXT: Function: main: error: Layout of LinAlgMatrixLoadFromDescriptor must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M8N8U2S0
  %9 = call %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M8N8U2S0(i32 -2147483634, %dx.types.Handle %6, i32 0, i32 0, i32 %8, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %10 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  ; CHECK-NEXT: Function: main: error: Stride of LinAlgMatrixLoadFromDescriptor must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U2S0
  %11 = call %dx.types.LinAlgMatrixC8M16N16U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U2S0(i32 -2147483634, %dx.types.Handle %10, i32 0, i32 %8, i32 4, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %12 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  ; No error expected for non-imm arg stride on row/col layout
  %13 = call %dx.types.LinAlgMatrixC8M4N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N8U2S0(i32 -2147483634, %dx.types.Handle %12, i32 0, i32 %8, i32 0, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  ret void
  ; CHECK-NEXT: Validation failed.
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M4N4U2S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M4N4U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M8N8U2S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N16U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U2S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M4N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N8U2S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone }

!dx.targetTypes = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}
!dx.version = !{!6}
!dx.valver = !{!6}
!dx.shaderModel = !{!7}
!dx.resources = !{!8}
!dx.entryPoints = !{!11}

!0 = !{%dx.types.LinAlgMatrixC8M4N4U2S2 undef, i32 8, i32 4, i32 4, i32 2, i32 2}
!1 = !{%dx.types.LinAlgMatrixC8M4N4U2S0 undef, i32 8, i32 4, i32 4, i32 2, i32 0}
!2 = !{%dx.types.LinAlgMatrixC8M8N8U2S0 undef, i32 8, i32 8, i32 8, i32 2, i32 0}
!3 = !{%dx.types.LinAlgMatrixC8M16N16U2S0 undef, i32 8, i32 16, i32 16, i32 2, i32 0}
!4 = !{%dx.types.LinAlgMatrixC8M4N8U2S0 undef, i32 8, i32 4, i32 8, i32 2, i32 0}
!5 = !{!"dxc(private) 1.9.0.5397 (linalg-validation-matrixaccumulate, b4a61ea75-dirty)"}
!6 = !{i32 1, i32 10}
!7 = !{!"cs", i32 6, i32 10}
!8 = !{!9, null, null, null}
!9 = !{!10}
!10 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!11 = !{void ()* @main, !"main", null, !8, !12}
!12 = !{i32 0, i64 16, i32 4, !13}
!13 = !{i32 1, i32 1, i32 1}
