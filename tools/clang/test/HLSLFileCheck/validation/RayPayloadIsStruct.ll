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
; export void BadAnyHit(inout float f, in Attributes a) {
;   f += a.b.x;
; }
; 
; [shader("closesthit")] void ClosestHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.y;
; }
; 
; export void BadClosestHit(inout float f, in Attributes a) {
;   f += a.b.y;
; }
; 
; [shader("miss")] void MissProto(inout Payload p) { p.f += 1.0; }
; 
; export void BadMiss(inout float f) {
;   f += 1.0;
; }

; CHECK: Argument 'f' must be a struct type for payload in shader function 'BadAny'
; CHECK: Argument 'f' must be a struct type for payload in shader function 'BadClosest'
; CHECK: Argument 'f' must be a struct type for payload in shader function 'BadMiss'


; ModuleID = '<stdin>'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.Attributes = type { <2 x float> }

; Function Attrs: nounwind
define void @BadAny(float* noalias nocapture dereferenceable(4) %f, %struct.Attributes* nocapture readonly %a) #0 {
  ret void
}

; Function Attrs: nounwind
define void @BadClosest(float* noalias nocapture dereferenceable(4) %f, %struct.Attributes* nocapture readonly %a) #0 {
  ret void
}

; Function Attrs: nounwind
define void @BadMiss(float* noalias nocapture dereferenceable(4) %f) #0 {
  ret void
}

attributes #0 = { nounwind }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4, !7, !9}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 7}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{void (float*, %struct.Attributes*)* @BadAny, !"BadAny", null, null, !5}
!5 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!6 = !{i32 0}
!7 = !{void (float*, %struct.Attributes*)* @BadClosest, !"BadClosest", null, null, !8}
!8 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!9 = !{void (float*)* @BadMiss, !"BadMiss", null, null, !10}
!10 = !{i32 8, i32 11, i32 6, i32 4, i32 5, !6}
