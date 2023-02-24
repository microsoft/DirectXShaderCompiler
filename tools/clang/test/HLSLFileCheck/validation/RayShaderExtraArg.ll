; RUN: %dxv %s | FileCheck %s

; Test based on IR generated from the following HLSL:
; struct Payload {
;   float f;
; };
; 
; struct Attributes {
;   float2 b;
; };
; 
; struct Param {
;   float f;
; };
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
;     [shader("callable")] void CallableProto(inout Param p) {
;   p.f += 1.0;
; }
; 
; export void BadAnyHit(inout Payload p, in Attributes a, float f) { p.f += f; }
; 
; export void BadClosestHit(inout Payload p, in Attributes a, float f) {
;   p.f += f;
; }
; 
; export void BadMiss(inout Payload p, float f) { p.f += f; }
; 
; export void BadCallable(inout Param p, float f) { p.f += f; }

; CHECK: Extra argument 'f' not allowed for shader function 'BadAnyHit'
; CHECK: Extra argument 'f' not allowed for shader function 'BadClosestHit'
; CHECK: Extra argument 'f' not allowed for shader function 'BadMiss'
; CHECK: Extra argument 'f' not allowed for shader function 'BadCallable'

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.Payload = type { float }
%struct.Attributes = type { <2 x float> }
%struct.Param = type { float }

; Function Attrs: nounwind
define void @BadAnyHit(%struct.Payload* noalias nocapture %p, %struct.Attributes* nocapture readnone %a, float %f) #0 {
  ret void
}

; Function Attrs: nounwind
define void @BadClosestHit(%struct.Payload* noalias nocapture %p, %struct.Attributes* nocapture readnone %a, float %f) #0 {
  ret void
}

; Function Attrs: nounwind
define void @BadMiss(%struct.Payload* noalias nocapture %p, float %f) #0 {
  ret void
}

; Function Attrs: nounwind
define void @BadCallable(%struct.Param* noalias nocapture %p, float %f) #0 {
  ret void
}

attributes #0 = { nounwind }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4, !7, !9, !11}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 7}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{void (%struct.Payload*, %struct.Attributes*, float)* @BadAnyHit, !"BadAnyHit", null, null, !5}
!5 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!6 = !{i32 0}
!7 = !{void (%struct.Param*, float)* @BadCallable, !"BadCallable", null, null, !8}
!8 = !{i32 8, i32 12, i32 6, i32 4, i32 5, !6}
!9 = !{void (%struct.Payload*, %struct.Attributes*, float)* @BadClosestHit, !"BadClosestHit", null, null, !10}
!10 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!11 = !{void (%struct.Payload*, float)* @BadMiss, !"BadMiss", null, null, !12}
!12 = !{i32 8, i32 11, i32 6, i32 4, i32 5, !6}
