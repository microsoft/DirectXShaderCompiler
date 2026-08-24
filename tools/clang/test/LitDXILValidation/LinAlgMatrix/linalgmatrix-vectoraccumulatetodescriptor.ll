; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%struct.ByteAddressBuffer = type { i32 }
%struct.RWByteAddressBuffer = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  ; okay
  call void @dx.op.linAlgVectorAccumulateToDescriptor.v4f32(i32 -2147483617, %dx.types.Handle %3, i32 0, i32 64, <4 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00>)  ; LinAlgVectorAccumulateToDescriptor(handle,offset,align,vector)
  %4 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer

  ; CHECK: Function: main: error: LinAlgVectorAccumulateToDescriptor requires RWByteAddressBuffer.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgVectorAccumulateToDescriptor.v4f32
  call void @dx.op.linAlgVectorAccumulateToDescriptor.v4f32(i32 -2147483617, %dx.types.Handle %4, i32 0, i32 192, <4 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00>)  ; LinAlgVectorAccumulateToDescriptor(handle,offset,align,vector)
  %5 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %6 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %5, i32 0, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %7 = extractvalue %dx.types.ResRet.i32 %6, 0
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  ; CHECK-NEXT: Function: main: error: Align of LinAlgVectorAccumulateToDescriptor must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgVectorAccumulateToDescriptor.v4f32
  call void @dx.op.linAlgVectorAccumulateToDescriptor.v4f32(i32 -2147483617, %dx.types.Handle %8, i32 0, i32 %7, <4 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00>)  ; LinAlgVectorAccumulateToDescriptor(handle,offset,align,vector)
  %9 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  ; CHECK-NEXT: Function: main: error: parameter 'Align' must be greater than 0, got 0
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgVectorAccumulateToDescriptor.v4f32
  call void @dx.op.linAlgVectorAccumulateToDescriptor.v4f32(i32 -2147483617, %dx.types.Handle %9, i32 0, i32 0, <4 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00>)  ; LinAlgVectorAccumulateToDescriptor(handle,offset,align,vector)
  %10 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  ; CHECK-NEXT: Function: main: error: parameter 'Align' must be a multiple of 64, got 199
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgVectorAccumulateToDescriptor.v4f32
  call void @dx.op.linAlgVectorAccumulateToDescriptor.v4f32(i32 -2147483617, %dx.types.Handle %10, i32 0, i32 199, <4 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00>)  ; LinAlgVectorAccumulateToDescriptor(handle,offset,align,vector)

  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind
declare void @dx.op.linAlgVectorAccumulateToDescriptor.v4f32(i32, %dx.types.Handle, i32, i32, <4 x float>) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.entryPoints = !{!8}

!0 = !{!"dxc(private) 1.9.0.5458 (linalg-vali-vecaccumtodescriptor, 9aadb5141-dirty)"}
!1 = !{i32 1, i32 10}
!2 = !{!"cs", i32 6, i32 10}
!3 = !{!4, !6, null, null}
!4 = !{!5}
!5 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!6 = !{!7}
!7 = !{i32 0, %struct.RWByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!8 = !{void ()* @main, !"main", null, !3, !9}
!9 = !{i32 0, i64 8598323216, i32 4, !10}
!10 = !{i32 1, i32 1, i32 1}

