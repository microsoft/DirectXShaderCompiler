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
; [shader("anyhit")] void AnyHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.x;
; }
; 
; export void BadAnyHit(inout Payload p, in float a) {
;   p.f += a;
; }
; 
; [shader("closesthit")] void ClosestHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.y;
; }
; 
; export void BadClosestHit(inout Payload p, in float a) {
;   p.f += a;
; }

; CHECK: Argument 'a' must be a struct type for attributes in shader function 'BadAnyHit'
; CHECK: Argument 'a' must be a struct type for attributes in shader function 'BadClosest'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.Payload = type { float }

; Function Attrs: nounwind
define void @BadAnyHit(%struct.Payload* noalias nocapture %p, float %a) #0 {
  ret void
}

; Function Attrs: nounwind
define void @BadClosest(%struct.Payload* noalias nocapture %p, float %a) #0 {
  ret void
}

attributes #0 = { nounwind }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4, !7}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 7}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{void (%struct.Payload*, float)* @BadAnyHit, !"BadAnyHit", null, null, !5}
!5 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!6 = !{i32 0}
!7 = !{void (%struct.Payload*, float)* @BadClosest, !"BadClosest", null, null, !8}
!8 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !6}

