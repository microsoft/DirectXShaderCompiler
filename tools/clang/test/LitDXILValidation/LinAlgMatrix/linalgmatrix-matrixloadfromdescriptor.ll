; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s
target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
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
%dx.types.LinAlgMatrixC8M16N16U0S0 = type { i8* }
%struct.ByteAddressBuffer = type { i32 }
%struct.RWByteAddressBuffer = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer


  ; CHECK: Function: main: error: Matrix scope 'ThreadGroup' requires layout RowMajor or ColumnMajor.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S2
  %4 = call %dx.types.LinAlgMatrixC8M4N4U2S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S2(i32 -2147483634, %dx.types.Handle %3, i32 0, i32 0, i32 4, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %5 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer


  ; CHECK-NEXT: Function: main: error: Matrix layout 'OuterProductOptimal' requires stride 0.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S0
  %6 = call %dx.types.LinAlgMatrixC8M4N4U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N4U2S0(i32 -2147483634, %dx.types.Handle %5, i32 0, i32 4, i32 4, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %7 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %8 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %7, i32 0, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %9 = extractvalue %dx.types.ResRet.i32 %8, 0


  ; CHECK-NEXT: Function: main: error: Layout of LinAlgMatrixLoadFromDescriptor must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M8N8U2S0
  %10 = call %dx.types.LinAlgMatrixC8M8N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M8N8U2S0(i32 -2147483634, %dx.types.Handle %7, i32 0, i32 0, i32 %9, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %11 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer


  ; CHECK-NEXT: Function: main: error: Stride of LinAlgMatrixLoadFromDescriptor must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U2S0
  %12 = call %dx.types.LinAlgMatrixC8M16N16U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U2S0(i32 -2147483634, %dx.types.Handle %11, i32 0, i32 %9, i32 4, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %13 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer


  ; No error expected for non-imm arg stride on row/col layout
  %14 = call %dx.types.LinAlgMatrixC8M4N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N8U2S0(i32 -2147483634, %dx.types.Handle %13, i32 0, i32 %9, i32 0, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %15 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer


  ; CHECK-NEXT: Function: main: error: parameter 'Align' must be a multiple of 128, got 215
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N8U2S0
  %16 = call %dx.types.LinAlgMatrixC8M4N8U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M4N8U2S0(i32 -2147483634, %dx.types.Handle %15, i32 0, i32 %9, i32 0, i32 215)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  %17 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  ; CHECK-NEXT: Function: main: error: Loading matrix with Thread scope requires SRV resource.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U0S0
  %18 = call %dx.types.LinAlgMatrixC8M16N16U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U0S0(i32 -2147483634, %dx.types.Handle %17, i32 0, i32 %9, i32 0, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)

  ; CHECK-NEXT: Validation failed.
  ret void
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

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N16U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N16U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone }

!dx.targetTypes = !{!0, !1, !2, !3, !4, !5}
!llvm.ident = !{!6}
!dx.version = !{!7}
!dx.valver = !{!7}
!dx.shaderModel = !{!8}
!dx.resources = !{!9}
!dx.entryPoints = !{!14}

!0 = !{%dx.types.LinAlgMatrixC8M4N4U2S2 undef, i32 8, i32 4, i32 4, i32 2, i32 2}
!1 = !{%dx.types.LinAlgMatrixC8M4N4U2S0 undef, i32 8, i32 4, i32 4, i32 2, i32 0}
!2 = !{%dx.types.LinAlgMatrixC8M8N8U2S0 undef, i32 8, i32 8, i32 8, i32 2, i32 0}
!3 = !{%dx.types.LinAlgMatrixC8M16N16U2S0 undef, i32 8, i32 16, i32 16, i32 2, i32 0}
!4 = !{%dx.types.LinAlgMatrixC8M4N8U2S0 undef, i32 8, i32 4, i32 8, i32 2, i32 0}
!5 = !{%dx.types.LinAlgMatrixC8M16N16U0S0 undef, i32 8, i32 16, i32 16, i32 0, i32 0}
!6 = !{!"dxc(private) 1.9.0.15416 (main, 27579abe5)"}
!7 = !{i32 1, i32 10}
!8 = !{!"cs", i32 6, i32 10}
!9 = !{!10, !12, null, null}
!10 = !{!11}
!11 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!12 = !{!13}
!13 = !{i32 0, %struct.RWByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!14 = !{void ()* @main, !"main", null, !9, !15}
!15 = !{i32 0, i64 8598323216, i32 4, !16}
!16 = !{i32 1, i32 1, i32 1}

