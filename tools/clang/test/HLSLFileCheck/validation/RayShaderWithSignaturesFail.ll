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
; [shader("anyhit")] void AnyHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.x;
; }
; 
; [shader("closesthit")]
; void ClosestHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.y;
; }
; 
; [shader("miss")] void MissProto(inout Payload p) { p.f += 1.0; }
; 
; [shader("callable")] void CallableProto(inout Param p) { p.f += 1.0; }
; 
; [shader("vertex")] float VSOutOnly() : OUTPUT { return 1; }
; 
; [shader("vertex")] void VSInOnly(float f : INPUT) : OUTPUT {}
; 
; [shader("vertex")] float VSInOut(float f : INPUT) : OUTPUT { return f; }

; CHECK: Ray tracing shader 'RayGen' should not have any shader signatures
; CHECK: Ray tracing shader 'Intersection' should not have any shader signatures
; CHECK: Ray tracing shader 'AnyHit' should not have any shader signatures
; CHECK: Ray tracing shader 'ClosestHit' should not have any shader signatures
; CHECK: Ray tracing shader 'Miss' should not have any shader signatures
; CHECK: Ray tracing shader 'Callable' should not have any shader signatures

; ModuleID = '<stdin>'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.Payload = type { float }
%struct.Attributes = type { <2 x float> }
%struct.Param = type { float }

; Function Attrs: nounwind readnone
define void @RayGen() #0 {
  ret void
}

; Function Attrs: nounwind readnone
define void @Intersection() #0 {
  ret void
}

; Function Attrs: nounwind
define void @AnyHit(%struct.Payload* noalias nocapture %p, %struct.Attributes* nocapture readonly %a) #1 {
  ret void
}

; Function Attrs: nounwind
define void @ClosestHit(%struct.Payload* noalias nocapture %p, %struct.Attributes* nocapture readonly %a) #1 {
  ret void
}

; Function Attrs: nounwind
define void @Miss(%struct.Payload* noalias nocapture %p) #1 {
  ret void
}

; Function Attrs: nounwind
define void @Callable(%struct.Param* noalias nocapture %p) #1 {
  ret void
}

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4, !10, !12, !14, !16, !18}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 3}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{void (%struct.Payload*, %struct.Attributes*)* @AnyHit, !"AnyHit", !5, null, !9}
!5 = !{!6, null, null}
!6 = !{!7}
!7 = !{i32 0, !"INPUT", i8 9, i8 0, !8, i8 0, i32 1, i8 1, i32 0, i8 0, null}
!8 = !{i32 0}
!9 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !8}
!10 = !{void (%struct.Param*)* @Callable, !"Callable", !5, null, !11}
!11 = !{i32 8, i32 12, i32 6, i32 4, i32 5, !8}
!12 = !{void (%struct.Payload*, %struct.Attributes*)* @ClosestHit, !"ClosestHit", !5, null, !13}
!13 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !8}
!14 = !{void ()* @Intersection, !"Intersection", !5, null, !15}
!15 = !{i32 8, i32 8, i32 5, !8}
!16 = !{void (%struct.Payload*)* @Miss, !"Miss", !5, null, !17}
!17 = !{i32 8, i32 11, i32 6, i32 4, i32 5, !8}
!18 = !{void ()* @RayGen, !"RayGen", !5, null, !19}
!19 = !{i32 8, i32 7, i32 5, !8}
