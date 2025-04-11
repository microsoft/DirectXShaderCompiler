; RUN: %dxv %s | FileCheck %s

; CHECK: Validation succeeded.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%struct.CustomAttrs = type { float, float }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.HitObject = type { i8* }
%struct.RaytracingAccelerationStructure = type { i32 }

@"\01?RTAS@@3URaytracingAccelerationStructure@@A" = external constant %dx.types.Handle, align 4

; Function Attrs: nounwind
define void @"\01?main@@YAXXZ"() #0 {
  %attrs = alloca %struct.CustomAttrs, align 4
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", align 4
  %rayQuery = call i32 @dx.op.allocateRayQuery(i32 178, i32 5)  ; AllocateRayQuery(constRayFlags)
  %handle = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %annotatedHandle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %handle, %dx.types.ResourceProperties { i32 16, i32 0 })  ; AnnotateHandle(res,props)  resource: RTAccelerationStructure
  call void @dx.op.rayQuery_TraceRayInline(i32 179, i32 %rayQuery, %dx.types.Handle %annotatedHandle, i32 0, i32 255, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00, float 0.000000e+00, float 0.000000e+00, float 9.999000e+03)  ; RayQuery_TraceRayInline(rayQueryHandle,accelerationStructure,rayFlags,instanceInclusionMask,origin_X,origin_Y,origin_Z,tMin,direction_X,direction_Y,direction_Z,tMax)

  ; Test HitObject_FromRayQuery (opcode 263)
  %r263 = call %dx.types.HitObject @dx.op.hitObject_FromRayQuery(i32 263, i32 %rayQuery)  ; HitObject_FromRayQuery(rayQueryHandle)

  ; Test HitObject_FromRayQueryWithAttrs (opcode 264)
  %r264 = call %dx.types.HitObject @dx.op.hitObject_FromRayQueryWithAttrs.struct.CustomAttrs(i32 264, i32 %rayQuery, i32 16, %struct.CustomAttrs* nonnull %attrs)  ; HitObject_FromRayQueryWithAttrs(rayQueryHandle,HitKind,CommittedAttribs)

  ret void
}

; Function Attrs: nounwind
declare i32 @dx.op.allocateRayQuery(i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.rayQuery_TraceRayInline(i32, i32, %dx.types.Handle, i32, i32, float, float, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.HitObject @dx.op.hitObject_FromRayQuery(i32, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.HitObject @dx.op.hitObject_FromRayQueryWithAttrs.struct.CustomAttrs(i32, i32, i32, %struct.CustomAttrs*) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #1

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32, %dx.types.Handle) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!dx.version = !{!0}
!dx.valver = !{!0}
!dx.shaderModel = !{!1}
!dx.resources = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!4, !5}

!0 = !{i32 1, i32 9}
!1 = !{!"lib", i32 6, i32 9}
!2 = !{!6, null, null, null}
!3 = !{i32 1, void ()* @"\01?main@@YAXXZ", !7}
!4 = !{null, !"", null, !2, !8}
!5 = !{void ()* @"\01?main@@YAXXZ", !"\01?main@@YAXXZ", null, null, !9}
!6 = !{!7}
!7 = !{i32 0, %struct.RaytracingAccelerationStructure* bitcast (%dx.types.Handle* @"\01?RTAS@@3URaytracingAccelerationStructure@@A" to %struct.RaytracingAccelerationStructure*), !"RTAS", i32 -1, i32 -1, i32 1, i32 16, i32 0, !10}
!8 = !{i32 0, i64 33554432}
!9 = !{i32 8, i32 7, i32 5, !11}
!10 = !{i32 0, i32 4}
!11 = !{i32 0}
