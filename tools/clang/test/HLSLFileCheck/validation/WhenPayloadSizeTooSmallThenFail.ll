; RUN: %dxilver 1.3 | %dxv %s | FileCheck %s

; Test based on IR generated from the following HLSL:
; struct Payload { float f; };
; 
; struct Attributes { float2 b; };
; 
; struct Param { float f; };
; 
; [shader("raygeneration")] void RayGenProto() { return; }
; 
; [shader("intersection")] void IntersectionProto() { return; }
; 
; [shader("anyhit")]
; void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }
; 
; [shader("closesthit")]
; void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }
; 
; [shader("miss")] void MissProto(inout Payload p) { p.f += 1.0; }
; 
; [shader("callable")] void CallableProto(inout Param p) { p.f += 1.0; }
; 
; struct BadPayload { float2 f; };
; 
; struct BadAttributes { float3 b; };
; 
; struct BadParam { float2 f; };
; 
; export void BadRayGen() { return; }
; 
; export void BadIntersection() { return; }
; 
; export void BadAnyHit(inout BadPayload p, in BadAttributes a) { p.f += a.b.x; }
; 
; export void BadClosestHit(inout BadPayload p, in BadAttributes a) {
;   p.f += a.b.y;
; }
; 
; export void BadMiss(inout BadPayload p) { p.f += 1.0; }
; 
; export void BadCallable(inout BadParam p) { p.f += 1.0; }

; CHECK: For shader 'AnyHit', payload size is smaller than argument's allocation size
; CHECK: For shader 'AnyHit', attribute size is smaller than argument's allocation size
; CHECK: For shader 'ClosestHit', payload size is smaller than argument's allocation size
; CHECK: For shader 'ClosestHit', attribute size is smaller than argument's allocation size
; CHECK: For shader 'Miss', payload size is smaller than argument's allocation size
; CHECK: For shader 'Callable', params size is smaller than argument's allocation size

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.BadPayload = type { <2 x float> }
%struct.BadAttributes = type { <3 x float> }
%struct.BadParam = type { <2 x float> }

; Function Attrs: nounwind readnone
define void @RayGen() #0 {
  ret void
}

; Function Attrs: nounwind readnone
define void @Intersection() #0 {
  ret void
}

; Function Attrs: nounwind
define void @AnyHit(%struct.BadPayload* noalias nocapture %p, %struct.BadAttributes* nocapture readonly %a) #1 {
  ret void
}

; Function Attrs: nounwind
define void @ClosestHit(%struct.BadPayload* noalias nocapture %p, %struct.BadAttributes* nocapture readonly %a) #1 {
  ret void
}

; Function Attrs: nounwind
define void @Miss(%struct.BadPayload* noalias nocapture %p) #1 {
  ret void
}

; Function Attrs: nounwind
define void @Callable(%struct.BadParam* noalias nocapture %p) #1 {
  ret void
}

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4, !7, !9, !11, !13, !15}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 3}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{void (%struct.BadPayload*, %struct.BadAttributes*)* @AnyHit, !"AnyHit", null, null, !5}
!5 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!6 = !{i32 0}
!7 = !{void (%struct.BadParam*)* @Callable, !"Callable", null, null, !8}
!8 = !{i32 8, i32 12, i32 6, i32 4, i32 5, !6}
!9 = !{void (%struct.BadPayload*, %struct.BadAttributes*)* @ClosestHit, !"ClosestHit", null, null, !10}
!10 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!11 = !{void ()* @Intersection, !"Intersection", null, null, !12}
!12 = !{i32 8, i32 8, i32 5, !6}
!13 = !{void (%struct.BadPayload*)* @Miss, !"Miss", null, null, !14}
!14 = !{i32 8, i32 11, i32 6, i32 4, i32 5, !6}
!15 = !{void ()* @RayGen, !"RayGen", null, null, !16}
!16 = !{i32 8, i32 7, i32 5, !6}
