; RUN: %dxv %s | FileCheck %s

; CHECK: Validation succeeded.


; shader hash: 55f96d488362429d3301238ab9ac07fd
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; BAB                                   UAV    byte         r/w      U0             u1     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.RWByteAddressBuffer = type { i32 }

@"\01?BAB@@3URWByteAddressBuffer@@A" = external constant %dx.types.Handle, align 4

; Function Attrs: nounwind
define void @"\01?main@@YAXXZ"() #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  call void @dx.op.barrierByMemoryType(i32 244, i32 1, i32 8)  ; BarrierByMemoryType(MemoryTypeFlags,SemanticFlags)
  %2 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %3 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.barrierByMemoryHandle(i32 245, %dx.types.Handle %3, i32 8)  ; BarrierByMemoryHandle(object,SemanticFlags)
  ret void
}

; Function Attrs: noduplicate nounwind
declare void @dx.op.barrierByMemoryType(i32, i32, i32) #1

; Function Attrs: noduplicate nounwind
declare void @dx.op.barrierByMemoryHandle(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32, %dx.types.Handle) #3

attributes #0 = { nounwind }
attributes #1 = { noduplicate nounwind }
attributes #2 = { nounwind readnone }
attributes #3 = { nounwind readonly }

!dx.version = !{!0}
!dx.valver = !{!0}
!dx.shaderModel = !{!1}
!dx.resources = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!4, !5}

!0 = !{i32 1, i32 9}
!1 = !{!"lib", i32 6, i32 9}
!2 = !{null, !6, null, null}
!3 = !{i32 1, void ()* @"\01?main@@YAXXZ", !7}
!4 = !{null, !"", null, !2, !8}
!5 = !{void ()* @"\01?main@@YAXXZ", !"\01?main@@YAXXZ", null, null, !9}
!6 = !{!10}
!7 = !{!11}
!8 = !{i32 0, i64 8589934608}
!9 = !{i32 8, i32 7, i32 5, !12}
!10 = !{i32 0, %struct.RWByteAddressBuffer* bitcast (%dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A" to %struct.RWByteAddressBuffer*), !"BAB", i32 0, i32 1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!11 = !{i32 1, !12, !12}
!12 = !{i32 0}
